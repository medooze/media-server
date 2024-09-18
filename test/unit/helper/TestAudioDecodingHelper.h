#ifndef TESTAUDIODECODING_H
#define	TESTAUDIODECODING_H
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
#include <string>
#include <queue>
#include "TestAudioCommon.h"


struct AudioEncodingParams
{
	int sampleRate;
	int numChannels;
	int64_t bitrate;
	AVSampleFormat sampleFmt;
	uint64_t channelLayout;
	AVCodecID codecId;
};

class AudioPacketGenerator
{
public:
	AudioPacketGenerator(AudioEncodingParams params, size_t numSamples, float frequency, int sampleRate) : 
		frequency(frequency),
		bitrate(params.bitrate),
		numChannels(params.numChannels),
		sampleRate(params.sampleRate),
		codecId(params.codecId),
		channelLayout(params.channelLayout),
		sampleFmt(params.sampleFmt),
		totalSamplesPerChannel(static_cast<size_t>(numSamples))
	{
		prepareEncoderContext();
		const enum AVSampleFormat *p = codec->sample_fmts;
		while (*p != AV_SAMPLE_FMT_NONE)
		{
			supportedFmts.insert(av_get_sample_fmt_name(*p));
			p++;
		}
	}

	~AudioPacketGenerator()
	{
		if(frame)  av_frame_free(&frame);
		if(pkt) av_packet_free(&pkt);
		if(codecCtx) avcodec_free_context(&codecCtx);
	}

	const std::queue<AVPacket*>& GenerateAVPackets(double amplitude)
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
	int64_t bitrate;
	AVCodecID codecId;
	AVSampleFormat sampleFmt;
	int numChannels;
	uint64_t channelLayout;
	// this stores generated sine tone in format set by user
	uint8_t** audioDataInTargetFmt;
	// srcData stores generated sine tone in flt format
	uint8_t** srcData;

	uint8_t* samples;

	AVCodec *codec = nullptr;
	AVCodecContext *codecCtx = nullptr;
	AVFrame *frame = nullptr;
	AVPacket *pkt = nullptr;
	SwrContext* swrCtx = nullptr;
	std::unordered_set<std::string> supportedFmts;
	// store generated encoded packets
	std::queue<AVPacket*> avPackets;
	
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

		swrCtx = swr_alloc();
		if (!swrCtx) 
			Error("could not allocate swr context\n");

		// generated sine tone in interleaved flt format, so use AV_SAMPLE_FMT_FLT
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
	   
		return 1;
	}

	void generateSineTone(float amplitude, float* audData) 
	{
		for(int i=0;i<totalSamplesPerChannel;++i)
		{
			float sample = amplitude*sin( 2.0 * M_PI * frequency * i / sampleRate);
			for(int ch=0;ch<numChannels;++ch)
				audData[i*numChannels+ch] = sample;
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
};

#endif	/* TESTAUDIODECODING_H */