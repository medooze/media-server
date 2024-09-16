#include "TestCommon.h"
extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <aom/aom_encoder.h>
#include <aom/aom_codec.h>
#include <aom/aomcx.h>
}
#include "VideoDecoderWorker.h"
#include "VideoPipe.h"

struct EncodingParams
{
	int width;
	int height;
	int fps;
	int bframes;
};

class TestAV1Encoder
{
public:
	TestAV1Encoder(EncodingParams params, int numPackets) : 
		numPackets(numPackets),
		width(params.width),
		height(params.height),
		fps(params.fps),
		bframes(params.bframes),
		packetTimestamp(0),
		kfInterval(fps),
		packets(numPackets)  
	{
		if( prepareEncoderContext() == 0 ) 
		{
			Error("TestAV1Encoder failed to create encoder context\n");
			return;
		}
	}

	~TestAV1Encoder()
	{
		if (pic)
			aom_img_free(pic);
	}

	uint8_t generateLumaPixelVal(int imageIdx) 
	{
		return (10 * imageIdx + 1) % 256;
	} 
	
	void generateAV1EncodedPackets()
	{
		for (int i = 0; i < numPackets; ++i)
		{
			uint64_t pts = i, dur = 1;
			int x, y;
			/* Y */
			for (y = 0; y < height; y++)
			{
				for (x = 0; x < width; x++)
				{
					pic->planes[AOM_PLANE_Y][y*pic->stride[AOM_PLANE_Y] + x] = generateLumaPixelVal(i);
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
					pic->planes[AOM_PLANE_U][y*pic->stride[AOM_PLANE_U] + x] =  (128 + y + i * 2) % 256;
					pic->planes[AOM_PLANE_V][y*pic->stride[AOM_PLANE_V] + x] =  (64 + x + i * 5) % 256;
				}
			}
			int flags = 0;
			if (kfInterval > 0 && i % kfInterval == 0)
				flags |= AOM_EFLAG_FORCE_KF;
			encode(pic, pts, dur, flags);
		}
		// flush encoder
		while (encode(nullptr, -1, 0, 0)) continue;
	}

	auto getEncodedPackets() const
	{
		return std::move(packets);
	}

private:
	int width;
	int height;
	int fps;
	int bframes;
	int kfInterval;
	int numPackets;
	int packetTimestamp;
	int packetIdx = 0;
	std::vector<std::vector<uint8_t>> packets;

	aom_codec_ctx_t		encoder = {};
	aom_codec_enc_cfg_t	config = {};
	aom_image_t* pic = nullptr;

	int prepareEncoderContext()
	{
		int ret;
		ret = aom_codec_enc_config_default(aom_codec_av1_cx(), &config, 0);
		if(ret != AOM_CODEC_OK)
		{
			return Error("failed to configure av1 encoder");
		}
		config.g_w = width;
		config.g_h = height;
		config.g_timebase.num = 1;
		config.g_timebase.den = fps;
		
		ret = aom_codec_enc_init(&encoder, aom_codec_av1_cx(), &config, 0);
	  
		if (ret != AOM_CODEC_OK)
		{
			return Error("av1 ecoder failed to init\n");
		}
		// hardcode color space for now, maybe parameterize it later if tests are required
		ret = aom_codec_control(&encoder, AV1E_SET_MATRIX_COEFFICIENTS, AVCOL_SPC_BT709);
		if (ret != AOM_CODEC_OK)
		{
			return Error("av1 ecoder failed to set color space\n");
		}
		// hardcode color range for now, maybe parameterize it later if tests are required
		ret = aom_codec_control(&encoder, AV1E_SET_COLOR_RANGE, AOM_CR_STUDIO_RANGE);
		if (ret != AOM_CODEC_OK)
		{
			return Error("av1 ecoder failed to set color range\n");
		}

		pic = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, width, height, 32);
		if (!pic)
		{
			return Error("failed to alloc source image\n");
		}
		
		return 1;
	}

	void copyPackets(const aom_codec_cx_pkt_t* pkt)
	{
		if (pkt->data.frame.sz == 0) return;
		std::vector<uint8_t> currPkt = packets[packetIdx];
		uint8_t* start = reinterpret_cast<uint8_t*>(pkt->data.frame.buf);
		std::copy(start, start + pkt->data.frame.sz, std::back_inserter(packets[packetIdx++]));
	}
   
	int encode(aom_image_t* pic, uint64_t pts, uint64_t dur, int flags)
	{
		
		int gotPkts = 0;
		if (aom_codec_encode(&encoder, pic, pts, dur, flags) != AOM_CODEC_OK)
			return Error("Failed to encode frame %s - %s\n", aom_codec_error(&encoder), aom_codec_error_detail(&encoder));

		aom_codec_iter_t iter = nullptr;
		const aom_codec_cx_pkt_t* pkt = nullptr;

		while ((pkt = aom_codec_get_cx_data(&encoder, &iter)) != nullptr)
		{
			gotPkts = 1;
			if (pkt->kind == AOM_CODEC_CX_FRAME_PKT)
			{
				copyPackets(pkt);
			}
		}
		return gotPkts;
	}
};

void runAV1Test(const char *codecName, const EncodingParams &params, int numPackets)
{
	uint64_t frameTime = 1E6 / params.fps;
	VideoCodec::Type type = VideoCodec::GetCodecForName(codecName);
	
	TestAV1Encoder av1Enc(params, numPackets);
	av1Enc.generateAV1EncodedPackets();
	auto packets = av1Enc.getEncodedPackets();
   
	std::vector<uint8_t> generatedPixelValue;
	std::vector<uint8_t> expectedPixelValue;

	std::vector<int> generatedPTS;
	std::vector<int> expectedPTS;

	for (int i = 0; i < numPackets; ++i)
	{
		// same equation used in generateAVPackets function
		expectedPixelValue.push_back(av1Enc.generateLumaPixelVal(i));
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
		auto packet = packets[i];
	
		auto frame = std::make_unique<VideoFrame>(type, packet.size());
		frame->SetTime(i);
		frame->SetTimestamp(i);
		frame->SetPresentationTimestamp(i);
		// since we expect pts diff to be 1, so clockrate should be fps
		frame->SetClockRate(params.fps);
		frame->AppendMedia(packet.data(), packet.size());
		// push generated VideoFrame to decoder worker
		worker.onMediaFrame(*frame);
		// get decoded frame through video pipe
		auto pic = vidPipe.GrabFrame(frameTime / 1000);
		if (pic)
		{
			generatedPTS.push_back(pic->GetTimestamp());
			generatedPixelValue.push_back(pic->GetPlaneY().GetData()[0]);
			VideoBuffer::ColorRange cr = pic->GetColorRange();
			VideoBuffer::ColorSpace cs = pic->GetColorSpace();
			ASSERT_EQ(cr, VideoBuffer::ColorRange::Partial) << "wrong color range";
			ASSERT_EQ(cs, VideoBuffer::ColorSpace::BT709) << "wrong color space";
		}
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
		EXPECT_NEAR(expectedPixelValue[i], generatedPixelValue[i], 1) << "pixel value differ at index " << i; 
	}
}

TEST(TestAV1, AV1Decoder)
{
	int height = 360, width = 640, fps = 30, bframes = 0;
	EncodingParams params = {
		width,
		height,
		fps,
		bframes
		};

	int numPackets = params.fps + 1;
	runAV1Test("AV1", params, numPackets);
}
