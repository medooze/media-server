/* 
 * File:   aacencoder.cpp
 * Author: Sergio
 * 
 * Created on 20 de junio de 2013, 10:46
 */

#include "AACEncoder.h"
#include "log.h"

AACEncoder::AACEncoder(const Properties &properties): audioFrame(AudioCodec::AAC)
{
	//NO ctx yet
	swr = NULL;
	ctx = NULL;
	frame = NULL;
	///Set type
	type = AudioCodec::AAC;

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);

	// Check codec
	if(!codec)
	{
		Error("-AACEncoder::AACEncoder() No encoder found\n");
		return;
	}
	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);

	//Check
	if (ctx == NULL)
	{
		Error("-AACEncoder::AACEncoder() could not allocate context.\n");
		return;
	}

	for (Properties::const_iterator it = properties.begin(); it != properties.end(); ++it)
		Debug("-AACEncoder::AACEncoder() | Setting property [%s:%s]\n", it->first.c_str(), it->second.c_str());

	//Set params
	ctx->channels		= 1;
	ctx->channel_layout	= AV_CH_LAYOUT_MONO;
	ctx->sample_fmt		= AV_SAMPLE_FMT_FLTP;

	//Set parames
	ctx->sample_rate	= properties.GetProperty("aac.samplerate"	,8000);
	ctx->bit_rate		= properties.GetProperty("aac.bitrate"		,ctx->sample_rate*3);
	ctx->thread_count	= 1;

	//Allow experimental codecs
	ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	{
		//Error
	        Error("-AACEncoder::AACEncoder() could not open codec\n");
		//Exit
		return;
	}

	//Get the number of samples
	numFrameSamples = ctx->frame_size;

	//Create context for converting sample formats
	swr = swr_alloc();

	//Set options
	av_opt_set_int(swr, "in_channel_layout",  AV_CH_LAYOUT_MONO,  0);
	av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_MONO,  0);
	av_opt_set_int(swr, "in_sample_rate",     ctx->sample_rate,   0);
	av_opt_set_int(swr, "out_sample_rate",    ctx->sample_rate,   0);
	av_opt_set_int(swr, "in_sample_fmt",      AV_SAMPLE_FMT_S16,  0);
	av_opt_set_int(swr, "out_sample_fmt",     ctx->sample_fmt	, 0);

	//Open context
	swr_init(swr);

	//Temporal buffer samples
	samplesNum = numFrameSamples;

	//Allocate float samples
	av_samples_alloc(&samples, &samplesSize, ctx->channels, samplesNum, ctx->sample_fmt, 0);

	//Create frame
	frame = av_frame_alloc();
	//Set defaults
	frame->nb_samples     = ctx->frame_size;
	frame->format         = ctx->sample_fmt;
	frame->channel_layout = ctx->channel_layout;
	frame->channels = ctx->channels;
	frame->sample_rate = ctx->sample_rate;

	audioFrame.DisableSharedBuffer();
	//Log
	Log("-AACEncoder::AACEncoder() Encoder open with frame size %d.\n", numFrameSamples);
}

AACEncoder::~AACEncoder()
{
	//Check
	if (ctx)
	{
		//Close
		avcodec_close(ctx);
		av_free(ctx);
	}
	if(swr)
		swr_free(&swr);
	if (samples)
		av_freep(&samples);
	if (frame)
		av_frame_free(&frame);
}

AudioFrame* AACEncoder::Encode(const AudioBuffer::const_shared& audioBuffer)
{
	AVPacket pkt;
	int ret;

	auto in = audioBuffer->GetData();
	auto inLen = audioBuffer->GetNumSamples() * audioBuffer->GetNumChannels();	

	if (!inLen)
		return 0;
	
	if (ctx == NULL)
		Error("-AACEncoder::Encode() no context.\n");
		return nullptr;

	//If not enought samples
	if (inLen!=numFrameSamples)
		//Exit
		Error("-AACEncoder::Encode() sample size %d is not correct. Should be %d\n", inLen, numFrameSamples);
		return nullptr;
	//Convert
	int len = swr_convert(swr, &samples, samplesNum, (const BYTE**)&in, inLen);
	//Check 
	if (avcodec_fill_audio_frame(frame, ctx->channels, ctx->sample_fmt, (BYTE*)samples, samplesSize, 0)<0)
		Error("-AACEncoder::Encode() could not fill audio frame \n");
		return nullptr;

	//Reset packet
	av_init_packet(&pkt);
	//Set output
	frame->nb_samples = len;
	
	//Send frame for encoding
	ret = avcodec_send_frame(ctx, frame);
	if (ret < 0)
	{
		Error("-AACEncoder::Encode() could not encode audio frame\n");
		return nullptr;
	}

	ret = avcodec_receive_packet(ctx, &pkt);
	// If the encoder asks for more data to be able to provide an
	// encoded frame, or the last frame has been encoded
	if (ret == AVERROR(EAGAIN))
	{
		Warning("-AACEncoder::Encode() encoder needs more data or EOF received\n");
		return nullptr;
	}
	else if(ret < 0)
	{
		Error("-AACEncoder::Encode() could not get output packet\n");
		return nullptr;
	}
	else	
		audioFrame.AppendMedia(pkt.data, pkt.size);
	return &audioFrame;
} 
