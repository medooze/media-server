#include "TestCommon.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}
#include <unordered_set>
#include "log.h"
#include <cmath>

struct EncodingParams
{
    int sampleRate;
    int numChannels;
    int64_t bitrate;
    AVSampleFormat sampleFmt;
    uint64_t channelLayout;
    AVCodecID codecId;
};

class AVPacketGenerator
{
public:
    AVPacketGenerator(EncodingParams params, float duration, float frequency, int sampleRate) : 
        duration(duration),
        frequency(frequency),
        bitrate(params.bitrate),
        numChannels(params.numChannels),
        sampleRate(params.sampleRate),
        codecId(params.codecId),
        channelLayout(params.channelLayout),
        sampleFmt(params.sampleFmt),
        totalSamplesPerChannel(static_cast<size_t>(duration*sampleRate))
    {
        prepareEncoderContext();
        prepareDecoderContext();
        const enum AVSampleFormat *p = codec->sample_fmts;
        while (*p != AV_SAMPLE_FMT_NONE)
        {
            supportedFmts.insert(av_get_sample_fmt_name(*p));
            p++;
        }
    }

    ~AVPacketGenerator()
    {
        if(frame)  av_frame_free(&frame);
        if(pkt) av_packet_free(&pkt);
        if(codecCtx) avcodec_free_context(&codecCtx);
        if(avfc) avformat_free_context(avfc);
    }

    const std::queue<AVPacket *> &GenerateAVPackets(double amplitude)
    {
        generateSineTone(amplitude, (float*)srcData[0]);
        int bufferIdx = 0;
        int framePTS = 0;

        while( bufferIdx < totalSamplesPerChannel) 
        {
            int numSamples = std::min(codecCtx->frame_size, totalSamplesPerChannel - bufferIdx);
            int numSamplesInTargetFmt = swr_convert(swrCtx, audioDataInTargetFmt, numSamples, (const uint8_t **)srcData, numSamples);
            if(numSamplesInTargetFmt<0) 
            {
                Error("convert sample format failed\n");
                break;
            }
            else if (numSamplesInTargetFmt != numSamples) 
            {
                Warning("converted sample not equal to frame size\n");
            }
           
            frame->nb_samples = numSamplesInTargetFmt;
            frame->pts = framePTS;
            framePTS+=numSamplesInTargetFmt;

            int bufSize =  numSamplesInTargetFmt * numChannels * av_get_bytes_per_sample(codecCtx->sample_fmt);
            if(avcodec_fill_audio_frame(frame, codecCtx->channels, codecCtx->sample_fmt, audioDataInTargetFmt[0], bufSize, 0) < 0) 
            {
                Error("fill audio sample failed\n");
                break;
            }
            
            encode(frame, pkt);
            bufferIdx += numSamplesInTargetFmt;
            srcData[0] += numChannels * numSamplesInTargetFmt * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);

        }
        // flush encoder
        encode(nullptr, pkt);
        return avPackets;
    }
    void decode(AVPacket* input_packet) {

        int response = avcodec_send_packet(decoderCtx, input_packet);
        if (response < 0)
        {
            printf("Error while sending packet to decoder");
            exit(1);
        }
        
        AVFrame* decoded = av_frame_alloc();
        int len = 0;
        while (response >= 0)
        {
            response = avcodec_receive_frame(decoderCtx, decoded);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
            {
                break;
            }
            else if (response < 0)
            {
                printf("Error while receiving frame from decoder");
                exit(1);
            }
            AVFrame* cloned = av_frame_clone(decoded);
            decodedFrames.push(cloned);
            av_frame_unref(decoded);
            	
        }   
    }
    std::queue<AVFrame*>& getDecodedFrames() {
        return decodedFrames;
    }

    void GenerateOutputFile(double amplitude)
    {
        generateSineTone(amplitude, (float*)srcData[0]);

        if (!(avfc->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&avfc->pb, "./output.mp3", AVIO_FLAG_WRITE) < 0) {
                printf("could not open the output file");
                return;
            }
        }
        AVDictionary* muxer_opts = NULL;
        if (avformat_write_header(avfc, &muxer_opts) < 0) 
        {
            printf("an error occurred when opening output file"); return;
        }
        int bufferIdx = 0;
        int framePTS = 0;

        while( bufferIdx < totalSamplesPerChannel) 
        {
            int numSamples = std::min(codecCtx->frame_size, totalSamplesPerChannel - bufferIdx);
            int numSamplesInTargetFmt = swr_convert(swrCtx, audioDataInTargetFmt, numSamples, (const uint8_t **)srcData, numSamples);
            if(numSamplesInTargetFmt<0) 
            {
                Error("convert sample format failed\n");
                break;
            }
            else if (numSamplesInTargetFmt != numSamples) 
            {
                Warning("converted sample not equal to frame size\n");
            }

            frame->nb_samples = numSamplesInTargetFmt;
            frame->pts = framePTS;
            framePTS+=numSamplesInTargetFmt;
            
            int bufSize =  numSamplesInTargetFmt*numChannels*av_get_bytes_per_sample(codecCtx->sample_fmt);
            if(avcodec_fill_audio_frame(frame, codecCtx->channels, codecCtx->sample_fmt, audioDataInTargetFmt[0], bufSize, 0) < 0) 
            {
                Error("fill audio sample failed\n");
                break;
            }
           
            encodeAndWrite(frame, pkt);
            bufferIdx += numSamplesInTargetFmt;
            srcData[0] += numChannels * numSamplesInTargetFmt * av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);
        }
        // flush encoder
        encode(nullptr, pkt);
        av_write_trailer(avfc);
        return;
    }

    const std::unordered_set<std::string>& GetSupportedSampleFormat()
    {
        return supportedFmts;
    }
    bool IsValidSampleFormat(AVSampleFormat fmt)
    {
        if(supportedFmts.find(av_get_sample_fmt_name(fmt)) == supportedFmts.end())
            return false;
        return true;
    }
private:
    int sampleRate;
    int totalSamplesPerChannel;
    float frequency;
    float duration;
    int64_t bitrate;
    AVCodecID codecId;
    AVSampleFormat sampleFmt;
    int numChannels;
    uint64_t channelLayout;
    // this stores generated sine tone in format set by user
    uint8_t** audioDataInTargetFmt;
    // srcData stores generated sine tone in flt format
    uint8_t** srcData;

    AVCodec *codec = nullptr;
    AVCodecContext *codecCtx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    SwrContext* swrCtx = nullptr;
    std::unordered_set<std::string> supportedFmts;

    // test
    AVCodec *decoderCodec;
    AVCodecContext *decoderCtx;
    std::queue<AVFrame*> decodedFrames;
    // write generated packets to file
    AVFormatContext *avfc = nullptr;
    AVStream *audioStream = nullptr;

    // store generated encoded packets
    std::queue<AVPacket *> avPackets;
    
    int prepareDecoderContext()
    {
        decoderCodec = const_cast<AVCodec *>(avcodec_find_decoder(AV_CODEC_ID_MP3));
        if (!decoderCodec)
        {
            printf("failed to find the codec");
            exit(1);
        }
        // allocate codec context
        decoderCtx = avcodec_alloc_context3(decoderCodec);
        if (!decoderCtx)
        {
            printf("failed to alloc memory for codec context");
            exit(1);
        }
        decoderCtx->pkt_timebase = (AVRational){1, sampleRate};
    
        if (avcodec_open2(decoderCtx, decoderCodec, NULL) < 0)
        {
            printf("failed to open codec");
            exit(1);
        }
    }

    int prepareEncoderContext()
    {
        int ret;
        codec = avcodec_find_encoder(codecId);

        if (!codec)
            return Error("Codec not found");
    
        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx)
            return Error("Codec not alloc codec context");
 
        codecCtx->bit_rate = bitrate;
        codecCtx->sample_fmt = sampleFmt;
        codecCtx->sample_rate = sampleRate;
        codecCtx->channel_layout = channelLayout;
        codecCtx->channels = numChannels;
        codecCtx->time_base = (AVRational){1, sampleRate};

        if (!checkSampleFormat(codec, codecCtx->sample_fmt))
        {
            Error("Encoder does not support sample format %s\n", av_get_sample_fmt_name(codecCtx->sample_fmt));
            avcodec_free_context(&codecCtx);
            return -1;
        }
       
        if (avcodec_open2(codecCtx, codec, NULL) < 0)
            return Error("Could not open codec\n");
       
        pkt = av_packet_alloc();
        if (!pkt)
            return Error("Could not allocate video packet\n");
   
        frame = av_frame_alloc();
        if (!frame)
            return Error("Could not allocate video frame\n");
       
        frame->format = codecCtx->sample_fmt;
        frame->nb_samples = codecCtx->frame_size;
        frame->channels = codecCtx->channels;
        frame->channel_layout = codecCtx->channel_layout;

        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0)
            return Error("Could not allocate the video frame data\n");


        swrCtx = swr_alloc();
        if (!swrCtx) 
            Error("could not allocate swr context\n");

        // generated sine tone is interleaved float, so pick AV_SAMPLE_FMT_FLT as input sample format
        swrCtx = swr_alloc_set_opts(nullptr, 
                                    codecCtx->channel_layout, codecCtx->sample_fmt, codecCtx->sample_rate,
                                    codecCtx->channel_layout, AV_SAMPLE_FMT_FLT, sampleRate,
                                    0, nullptr);
        
        if(!swrCtx || swr_init(swrCtx) < 0) 
            return Error("could not allocate resampler context\n");
        
        int dstLineSize;
        if( av_samples_alloc_array_and_samples(&audioDataInTargetFmt, &dstLineSize, codecCtx->channels, codecCtx->frame_size, codecCtx->sample_fmt, 0) < 0 )
            return Error("allocate samples buffer failed\n");
        
        int srcLineSize;
        if( av_samples_alloc_array_and_samples(&srcData, &srcLineSize, codecCtx->channels, totalSamplesPerChannel, AV_SAMPLE_FMT_FLT, 0) < 0 )
            return Error("allocate samples buffer failed\n");
       
        avformat_alloc_output_context2(&avfc, NULL, NULL, "./output.mp3");
        if (!avfc)
            printf("could not allocate memory for output format");
       
        audioStream=avformat_new_stream(avfc, NULL);
        audioStream->time_base = codecCtx->time_base;
        ret = avcodec_parameters_from_context(audioStream->codecpar, codecCtx);
        return 1;
    }

    void generateSineTone(float amplitude, float* audData) 
    {
        for(int i=0;i<totalSamplesPerChannel;++i)
        {
            float sample = amplitude*sin( 2.0 * M_PI * frequency * i / sampleRate);
            for(int ch=0;ch<numChannels;++ch)
            {
                audData[i*numChannels+ch] = sample;
            }
        }

    }

    int checkSampleFormat(const AVCodec *codec, enum AVSampleFormat sampleFmt)
    {
        const enum AVSampleFormat *p = codec->sample_fmts;
        while (*p != AV_SAMPLE_FMT_NONE) {
            if (*p == sampleFmt)
                return 1;
            p++;
        }
        return 0;
    }

    int encode(AVFrame *input, AVPacket *pkt)
    {
        int ret;
        ret = avcodec_send_frame(codecCtx, input);
        if (ret < 0)
        {
            return Error("Error sending frame for encoding\n");
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
            avPackets.push(dst);
            av_packet_unref(pkt);
        }
        return 1;
    }

    int encodeAndWrite(AVFrame *input, AVPacket *pkt)
    {
        int ret;
        ret = avcodec_send_frame(codecCtx, input);
        if (ret < 0)
        {
            return Error("Error sending frame for encoding\n");
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

        
            int response = av_interleaved_write_frame(avfc, pkt);
            if (response != 0) 
            { 
                printf("Error %d while receiving packet from decoder\n", response); return -1;
            }

        }
        return 1;
    }
};

TEST(TestAudioCodec, MP3Decode)
{

    av_log_set_level(AV_LOG_DEBUG);
    uint64_t channelLayout = AV_CH_LAYOUT_STEREO;
    int sampleRate = 48000, numChannels = av_get_channel_layout_nb_channels(channelLayout);;
    int64_t bitrate = 192000;

    float durationInSec = 1.0, freqInHz = 1000.0;

    AVSampleFormat sampleFmt = AV_SAMPLE_FMT_FLT;
    EncodingParams params = {
        sampleRate,
        numChannels,
        bitrate,
        sampleFmt,
        channelLayout,
        AV_CODEC_ID_MP3};
   
    AVPacketGenerator avGenerator(params, durationInSec, freqInHz, sampleRate);
    if(avGenerator.IsValidSampleFormat(sampleFmt))
    {
        std::queue<AVPacket*> inputPackets = avGenerator.GenerateAVPackets(0.5);

        int numPackets = 0;
        while(!inputPackets.empty()) 
        {
            auto packet = inputPackets.front();
            auto tmp = packet->data;
            numPackets++;
            // avGenerator.decode(packet);
            inputPackets.pop();
            
        }
        printf("total packets count %d\n", numPackets);
        auto decodedFrames = avGenerator.getDecodedFrames();
        int totalSamples = 0;
        while(!decodedFrames.empty())
        {
            auto frame = decodedFrames.front();
            // printf("frame pkt pts:%ld, frame pkt duration:%llu\n",frame->pkt_pts, frame->pkt_duration);
            // float* sample0 = (float*)frame->extended_data[0];
            // // float* sample1 = (float*)frame->extended_data[1];
            // if(totalSamples == 0) {
            //     for(int i=0;i<frame->nb_samples;i++)
            //     {
            //         printf("frame data ch0:%lf\n", sample0[i]);
            //     }
            // }
            totalSamples+=frame->nb_samples;
            decodedFrames.pop();
        }
        printf("total samples decoded:%d\n", totalSamples);
        // avGenerator.GenerateOutputFile(0.5);

    }

}