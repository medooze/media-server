#ifndef TESTAUDIOENCODING_H
#define	TESTAUDIOENCODING_H
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
#include <vector>
#include "AudioBuffer.h"
#include "AudioEncoderWorker.h"
#include "audio.h"
#include <mutex>
#include "TestAudioCommon.h"

struct AudioPCMParams
{
	int numFrames;
	int decoderFrameSize;
	int sampleRate;
	int numChannels;
	int startPTS;
	float freq;
	float amplitude;
};

class AudioBufferGenerator
{
public:
	AudioBufferGenerator(int sampleRate, int numChannels, float frequency, float amplitude):
		sampleRate(sampleRate),
		numChannels(numChannels),
		frequency(frequency),
		amplitude(amplitude)
	{

	}
	~AudioBufferGenerator() = default;

	AudioBuffer::shared CreateAudioBuffer(int numSamples, int offset)
	{
		auto audioBuffer = std::make_shared<AudioBuffer>(numSamples, numChannels);
		std::vector<int16_t> interleaved(numSamples*numChannels, 0);
		
		for(int i=offset;i<numSamples+offset;++i)
		{
			float sample = amplitude*sin( 2.0 * M_PI * frequency * i / sampleRate);
			for(int ch=0;ch<numChannels;++ch)
				interleaved[(i-offset)*numChannels+ch] = (int16_t)(sample * (1 << 15));
			
		}
		audioBuffer->SetSamples(interleaved.data(), numSamples);
		return audioBuffer;
	}
private:
	int sampleRate;
	int numChannels;
	float frequency;
	float amplitude;
};

class AudioDecoderForTest
{
public:
	AudioDecoderForTest(AVCodecID codec, int sampleRate, int numChannels): 
		codecID(codec),
		sampleRate(sampleRate),
		numChannels(numChannels)
	{
		prepareDecoderContext();
	}
	~AudioDecoderForTest()
	{
		if (decoderCtx) 
		{
			avcodec_close(decoderCtx);
			av_free(decoderCtx);
		}
		
		if (pkt)
			av_packet_free(&pkt);
		if (decoded)
			av_frame_free(&decoded);
	}
	
	void Decode(const AudioFrame::const_shared& audioFrame) 
	{
		pkt->data = audioFrame ? (uint8_t*)audioFrame->GetData() : nullptr;
		pkt->size = audioFrame ? audioFrame->GetLength() : 0;

		int response = avcodec_send_packet(decoderCtx, pkt);

		if (response < 0)
		{
			printf("Error while sending packet to decoder:%d\n", response);
			exit(1);
		}
	   
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
			copyDecoded(decoded->extended_data, decoded->nb_samples, decoded->channels);
			av_frame_unref(decoded);
		}
	}

	std::vector<float> GetDecodedAudio() {return pcmAudio;};
	
private:
	AVCodecID codecID;
	int sampleRate;
	int numChannels;
	AVPacket* pkt;
	AVFrame* decoded;
	const AVCodec *decoder;
	AVCodecContext *decoderCtx;
	std::vector<float> pcmAudio;
	
	void prepareDecoderContext()
	{
		decoder = avcodec_find_decoder(codecID);
		if (!decoder)
		{
			printf("failed to find the codec");
			exit(1);
		}
		// allocate codec context
		decoderCtx = avcodec_alloc_context3(decoder);
		if (!decoderCtx)
		{
			printf("failed to alloc memory for codec context");
			exit(1);
		}
		decoderCtx->pkt_timebase = (AVRational){1, sampleRate};
		decoderCtx->channels = 1;
		decoderCtx->channel_layout = AV_CH_LAYOUT_MONO;
	
		if (avcodec_open2(decoderCtx, decoder, NULL) < 0)
		{
			printf("failed to open codec");
			exit(1);
		}
		pkt = av_packet_alloc();
		decoded = av_frame_alloc();
	}

	int copyDecoded(uint8_t** pcmData, uint32_t numSamples, int numChannels)
	{
		if(!pcmData || !*pcmData)
			return Error("invalid frame data pointer\n");
		
		int totalWrittenSamples = 0;
		for (size_t i = 0; i < numSamples; ++i)
		{
			//For each channel
			for (size_t ch = 0; ch < numChannels; ++ch)
			{
				pcmAudio.push_back(((float*)(pcmData[ch]))[i]);
				totalWrittenSamples++;
			}

		}
		return totalWrittenSamples/numChannels;		
	}
};


class MediaFrameListenerForAudioEncoderTest: public MediaFrame::Listener
{
public:
	MediaFrameListenerForAudioEncoderTest()
	{

	}
	~MediaFrameListenerForAudioEncoderTest() = default;
	virtual void onMediaFrame(const MediaFrame &frame) override
	{
		std::lock_guard<std::mutex> lock(mtx);
		numEncodedFrames++;
		frames.push(std::shared_ptr<AudioFrame>(static_cast<AudioFrame*>(frame.Clone())));
	}

	int GetNumEncodedFrames()
	{
		std::lock_guard<std::mutex> lock(mtx);
		int currNum = numEncodedFrames;
		return currNum;
	}
	std::queue<std::shared_ptr<AudioFrame>>& GetAudioFrames() {return frames;};

	virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame) override
	{
	}
private:
	int numEncodedFrames = 0;
	std::mutex mtx;
	std::queue<std::shared_ptr<AudioFrame>> frames;
};

class SineToneGenerator
{
public:
	SineToneGenerator(int sampleRate, int numChannels, float frequency, float amplitude, int frameSize):
		sampleRate(sampleRate),
		numChannels(numChannels),
		frequency(frequency),
		amplitude(amplitude),
		interleaved(frameSize*numChannels, 0)
	{

	}
	~SineToneGenerator() = default;

	SWORD* GenerateSineTone(int numSamples, int offset)
	{
		for(int i=offset;i<numSamples+offset;++i)
		{
			float sample = amplitude*sin( 2.0 * M_PI * frequency * i / sampleRate);
			for(int ch=0;ch<numChannels;++ch)
				interleaved[(i-offset)*numChannels+ch] = (int16_t)(sample * (1 << 15));
		}
		return interleaved.data();
	}
private:
	int sampleRate;
	int numChannels;
	float frequency;
	float amplitude;
	std::vector<int16_t> interleaved;
};


static void TestAudioEncoder(AudioCodec::Type codecType, const Properties& props, int sampleRate, int numChannels, int encoderFrameSize, const AudioPCMParams& params)
{
	AVCodecID codec;
	switch (codecType)
	{
		// case AudioCodec::Type::AAC:
		// 	codec = AV_CODEC_ID_AAC;
		// 	break;
		case AudioCodec::Type::OPUS:
			codec = AV_CODEC_ID_OPUS;
			break;
		default:
			Error("codec type not supported by test yet\n");
			return;
	}
	int numFrames = params.numFrames, decoderFrameSize = params.decoderFrameSize;
	// generated audio buffers are used for encoding
	AudioBufferGenerator audioBufferGen(params.sampleRate, params.numChannels, params.freq, params.amplitude);
	// simple decoder used to decode encoded audio for test purpose
	AudioDecoderForTest audioDecoderForTest(codec, sampleRate, numChannels);

	AudioPipe audPipe(sampleRate);

	AudioEncoderWorker encoderWorker;
	MediaFrame::Listener::shared myMFListener = std::make_shared<MediaFrameListenerForAudioEncoderTest>();
	encoderWorker.AddListener(myMFListener);
	encoderWorker.SetAudioCodec(codecType, props);
	encoderWorker.Init(&audPipe);
	
	encoderWorker.StartEncoding();
	
	audPipe.StartRecording(sampleRate);
	audPipe.StartPlaying(params.sampleRate, params.numChannels);
	
	// offset is used to create continuous sine tone that is stored in audio buffer
	int pts = params.startPTS, expectedEncoderPTS = params.startPTS, offset=0;
	for(int i = 0;i < numFrames;i++)
	{
		auto audioBuffer = audioBufferGen.CreateAudioBuffer(decoderFrameSize, offset);
		audioBuffer->SetTimestamp(pts);
		audPipe.PlayBuffer(audioBuffer);
		pts+=decoderFrameSize;
		offset+=decoderFrameSize;
	}
	auto mediaFrameListener = std::static_pointer_cast<MediaFrameListenerForAudioEncoderTest>(myMFListener);
	auto startTime = std::chrono::steady_clock::now();
	int maxWaitInSeconds = 2;
	// waiting for all frames are encoded
	while(1)
	{
		if(mediaFrameListener->GetNumEncodedFrames() == numFrames) break;
		auto currTime = std::chrono::steady_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currTime-startTime).count();
		if(elapsedTime>=maxWaitInSeconds) break;
	}
	// send encoded frames created by AudioEncoderWorker to a decoder 
	std::queue<std::shared_ptr<AudioFrame>>& audioFrames = mediaFrameListener->GetAudioFrames();
	std::vector<uint64_t> audioFramePTS;
	while(!audioFrames.empty())
	{
		auto currFrame = audioFrames.front();
		audioFramePTS.push_back(currFrame->GetTimestamp());
		audioDecoderForTest.Decode(currFrame);
		audioFrames.pop();
	}

	auto decodedAudio = audioDecoderForTest.GetDecodedAudio();
	auto numDecodedSamples = audioDecoderForTest.GetDecodedAudio().size();
	auto res = findPeakFrequency(decodedAudio.data(), sampleRate, numDecodedSamples);

	EXPECT_NEAR(res.first, params.freq, 10) << "unexpected difference in frequency between generated sine tone and decoded sine tone: expected = " << params.freq << ", actual = " << res.first; 
	EXPECT_NEAR(res.second, params.amplitude, params.amplitude*0.1) << "unexpected difference in amplitude between generated sine tone and decoded sine tone: expected = " << params.amplitude << ", actual = " << res.second; 

	int mod = sampleRate>params.sampleRate?sampleRate%params.sampleRate:params.sampleRate%sampleRate;
	if(mod!=0)
	{
		EXPECT_EQ(expectedEncoderPTS, audioFramePTS[0]) << "start pts differ at index";
		for (int i = 1; i < mediaFrameListener->GetNumEncodedFrames(); ++i)
		{
			int ptsDiff = audioFramePTS[i]-audioFramePTS[i-1];
			EXPECT_TRUE((ptsDiff >= encoderFrameSize-1) && (ptsDiff <= encoderFrameSize+1)) << "pts diff: " << ptsDiff;
		}
	}
	else
	{
		for (int i = 0; i < mediaFrameListener->GetNumEncodedFrames(); ++i)
		{
			EXPECT_EQ(expectedEncoderPTS, audioFramePTS[i]) << "audio pts differ at index " << i;
			expectedEncoderPTS += encoderFrameSize;
		}
	}
}
#endif	/* TESTAUDIOENCODING_H */
