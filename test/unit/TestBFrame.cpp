#include "TestCommon.h"
extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}
#include "VideoDecoderWorker.h"
#include "VideoPipe.h"

struct EncodingParams
{
    int width;
    int height;
    int fps;
    int bframes;
    AVPixelFormat pixelFmt;
    AVCodecID codecId;
};

class AVPacketGenerator
{
public:
    AVPacketGenerator(EncodingParams params, int numPackets) : 
        numPackets(numPackets),
        width(params.width),
        height(params.height),
        fps(params.fps),
        bframes(params.bframes),
        codecId(params.codecId),
        pixelFmt(params.pixelFmt),
        packetTimestamp(0)
    {
        prepareEncoderContext();
    }

    ~AVPacketGenerator()
    {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codecCtx);
    }

    uint8_t generateLumaPixelVal(int imageIdx) 
    {
        return (10 * imageIdx + 1) % 256;
    } 
    
    std::queue<AVPacket *> &generateAVPackets()
    {
        // Create dummy images with YUV420 format, this format stores the Y, Cb and Cr components in three separate planes. 
        // The luma plane comes first, so frame->data[0] points to Y plane, frame->data[1] and frame->data[2] point to chroma planes.
        for (int i = 0; i < numPackets; ++i)
        {
            int x, y;
            /* Y */
            for (y = 0; y < height; y++)
            {
                for (x = 0; x < width; x++)
                {
                    frame->data[0][y * frame->linesize[0] + x] = generateLumaPixelVal(i);
                }
            }
            /* Cb and Cr */
            // With this chosen pixel format, the chroma planes are subsampled by 2 in each direction which means luma lines contain half
            // of the number of pixels and bytes of the luma lines, and the chroma planes contain half of the number of lines of the luma plane. 
            // The chroma value are chosen randomly, it is sufficient to check luma pixal values for these tests.
            for (y = 0; y < height / 2; y++)
            {
                for (x = 0; x < width / 2; x++)
                {
                    frame->data[1][y * frame->linesize[1] + x] = (128 + y + i * 2) % 256;
                    frame->data[2][y * frame->linesize[2] + x] = (64 + x + i * 5) % 256;
                }
            }
            frame->pts = i;
            encode(frame, pkt);
        }
        // flush encoder
        encode(nullptr, pkt);
        return avPackets;
    }

private:
    int width;
    int height;
    int fps;
    const char *preset;
    int bframes;
    AVCodecID codecId;
    AVPixelFormat pixelFmt;

    AVCodec *codec;
    AVCodecContext *codecCtx;
    AVFrame *frame;
    AVPacket *pkt;

    int numPackets;
    int packetTimestamp;
    std::queue<AVPacket *> avPackets;

    int prepareEncoderContext()
    {
        int ret;
        codec = avcodec_find_encoder(codecId);

        if (!codec)
        {
            return Error("Codec not found");
        }

        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx)
        {
            return Error("Codec not alloc codec context");
        }
        codecCtx->width = width;
        codecCtx->height = height;
        codecCtx->time_base = (AVRational){1, fps};
        codecCtx->framerate = (AVRational){fps, 1};
        codecCtx->max_b_frames = bframes;
        codecCtx->pix_fmt = pixelFmt;

        if (avcodec_open2(codecCtx, codec, NULL) < 0)
        {
            return Error("Could not open codec");
        }

        pkt = av_packet_alloc();
        if (!pkt)
        {
            return Error("Could not allocate video packet");
        }

        frame = av_frame_alloc();
        if (!frame)
        {
            return Error("Could not allocate video frame");
        }

        frame->format = codecCtx->pix_fmt;
        frame->width = codecCtx->width;
        frame->height = codecCtx->height;
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0)
        {
            return Error("Could not allocate the video frame data");
        }
        return 1;
    }

    int encode(AVFrame *input, AVPacket *pkt)
    {
        int ret;
        ret = avcodec_send_frame(codecCtx, input);
        if (ret < 0)
        {
            return Error("Error sending frame for encoding");
        }

        while (ret >= 0)
        {
            ret = avcodec_receive_packet(codecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0)
            {
                return Error("Error during encoding");
            }

            AVPacket *dst = av_packet_clone(pkt);
            dst->dts = packetTimestamp++;
            avPackets.push(dst);
            av_packet_unref(pkt);
        }
        return 1;
    }
};

void runBFrameTest(const char *codecName, const EncodingParams &params, int numPackets)
{
    uint64_t frameTime = 1E6 / params.fps;
    VideoCodec::Type type = VideoCodec::GetCodecForName(codecName);

    AVPacketGenerator generator(params, numPackets);
    std::queue<AVPacket *> inputPackets = generator.generateAVPackets();

    std::vector<uint8_t> generatedPixelValue;
    std::vector<uint8_t> expectedPixelValue;

    std::vector<int> generatedPTS;
    std::vector<int> expectedPTS;

    for (int i = 0; i < numPackets; ++i)
    {
        // same equation used in generateAVPackets function
        expectedPixelValue.push_back(generator.generateLumaPixelVal(i));
        expectedPTS.push_back(i);
    }

    VideoPipe vidPipe;
    vidPipe.Init(0.0, 0);
    vidPipe.StartVideoCapture(params.width, params.height, params.fps);

    VideoDecoderWorker worker;
    worker.AddVideoOutput(&vidPipe);
    worker.Start();

    for (int i = 0; i < numPackets; ++i)
    {
        auto packet = inputPackets.front();
        auto frame = std::make_unique<VideoFrame>(type, packet->size);
        frame->SetTime(packet->dts);
        frame->SetTimestamp(packet->dts);
        frame->SetPresentationTimestamp(packet->pts);
        frame->SetClockRate(1000);
        frame->AppendMedia(packet->data, packet->size);
        // push generated VideoFrame to decoder worker
        worker.onMediaFrame(*frame);
        // get decoded frame through video pipe
        auto pic = vidPipe.GrabFrame(frameTime / 1000);
        if (pic)
        {
            generatedPTS.push_back(pic->GetTimestamp());
            generatedPixelValue.push_back(pic->GetPlaneY().GetData()[0]);
        }
        av_packet_free(&packet);
        inputPackets.pop();
    }

    // flush the decoder
    auto frame = std::make_unique<VideoFrame>(type, 0);
    worker.onMediaFrame(*frame);
    while (auto pic = vidPipe.GrabFrame(frameTime / 1000))
    {
        generatedPTS.push_back(pic->GetTimestamp());
        generatedPixelValue.push_back(pic->GetPlaneY().GetData()[0]);
    }

    ASSERT_EQ(generatedPTS.size(), expectedPTS.size()) << "PTS input frames and output frames are of unequal length";
    ASSERT_EQ(generatedPixelValue.size(), expectedPixelValue.size()) << "input frames and output frames are of unequal length";

    for (int i = 0; i < expectedPTS.size(); ++i)
    {
        EXPECT_EQ(expectedPTS[i], generatedPTS[i]) << "pts differ at index " << i;
    }

    for (int i = 0; i < expectedPixelValue.size(); ++i)
    {
        EXPECT_EQ(expectedPixelValue[i], generatedPixelValue[i]) << "pixel value differ at index " << i;
    }
}

TEST(TestBFrame, H264CodecBFrame)
{

    int height = 360, width = 640, fps = 30, bframes = 4;
    EncodingParams params = {
        width,
        height,
        fps,
        bframes,
        AV_PIX_FMT_YUV420P,
        AV_CODEC_ID_H264};
    int numPackets = params.fps + 1;
    runBFrameTest("H264", params, numPackets);
}

TEST(TestBFrame, H265CodecBFrame)
{
    int height = 360, width = 640, fps = 30, bframes = 4;
    EncodingParams params = {
        width,
        height,
        fps,
        bframes,
        AV_PIX_FMT_YUV420P,
        AV_CODEC_ID_HEVC};
    int numPackets = params.fps + 1;
    runBFrameTest("H265", params, numPackets);
}

TEST(TestBFrame, VP8CodecBFrame)
{
    int height = 360, width = 640, fps = 30, bframes = 4;
    EncodingParams params = {
        width,
        height,
        fps,
        bframes,
        AV_PIX_FMT_YUV420P,
        AV_CODEC_ID_VP8};

    int numPackets = params.fps + 1;
    runBFrameTest("VP8", params, numPackets);
}

TEST(TestBFrame, VP9CodecBFrame)
{
    int height = 360, width = 640, fps = 30, bframes = 4;
    EncodingParams params = {
        width,
        height,
        fps,
        bframes,
        AV_PIX_FMT_YUV420P,
        AV_CODEC_ID_VP9};

    int numPackets = params.fps + 1;
    runBFrameTest("VP9", params, numPackets);
}
