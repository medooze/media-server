/* 
 * File:   NellyCodec.cpp
 * Author: Sergio
 * 
 * Created on 7 de diciembre de 2011, 23:29
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "NellyCodec.h"
#include "fifo.h"
#include "log.h"

NellyEncoder::NellyEncoder(const Properties &properties)
{
	//NO ctx yet
	ctx = NULL;
	frame = NULL;
	///Set type
	type = AudioCodec::NELLY8;

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_NELLYMOSER);

	// Check codec
	if(!codec)
		Error("No codec found\n");

	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);

	//Set params
	ctx->channels		= 1;
	ctx->sample_rate	= 8000;
	ctx->sample_fmt		= AV_SAMPLE_FMT_FLT;

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	         Error("could not open codec\n");

	//Get the number of samples
	numFrameSamples = ctx->frame_size;

	//Create frame
	frame = av_frame_alloc();
	//Set defaults
	frame->nb_samples     = ctx->frame_size;
	frame->format         = ctx->sample_fmt;
	frame->channel_layout = ctx->channel_layout;
}

NellyEncoder::~NellyEncoder()
{
	//Check
	if (ctx) {
		//Close
		avcodec_close(ctx);
		av_free(ctx);
		ctx = NULL;
	}
	if (frame)
		av_frame_free(&frame);
}

int NellyEncoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	AVPacket pkt;
	int got_output;

	SWORD buffer[512];
	DWORD len = 512;
	float bufferf[512];

	//Put samples back
	samples.push(in,inLen);
	//If not enought samples
	if (samples.length()<ctx->frame_size)
		//Exit
		return 0;
	//Get them
	samples.pop(buffer,ctx->frame_size);
	//For each one
	for (int i=0;i<ctx->frame_size;++i)
		//Convert to float
		bufferf[i] = buffer[i] * (1.0 / (1<<15));

	//Fill data
	if ( avcodec_fill_audio_frame(frame, ctx->channels, ctx->sample_fmt, (BYTE*)bufferf, ctx->frame_size*sizeof(SWORD), 0)<0)
		//Exit
		return Error("NELLY: could not fill audio frame\n");

	//Reset packet
	av_init_packet(&pkt);

	//Set output
	pkt.data = out;
	pkt.size = outLen;

	//Encode audio
	if (avcodec_encode_audio2(ctx, &pkt, frame, &got_output)<0)
		//Exit
		return Error("NELLY: could not encode audio frame\n");

	//Check if we got output
	if (!got_output)
		//Exit
		return Error("NELLY: could not get output packet\n");

	//Return encoded size
	return pkt.size;
}


NellyEncoder11Khz::NellyEncoder11Khz(const Properties &properties)
{
	//NO ctx yet
	ctx = NULL;
	resampler = NULL;
	frame = NULL;
	///Set type
	type = AudioCodec::NELLY11;

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_NELLYMOSER);

	// Check codec
	if(!codec)
		Error("No codec found\n");

	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);

	//The resampler to convert to WB
	int err;
	resampler = mcu_resampler_init(1, 8000, 11025, 5, &err);
	if (err)
	{
		resampler = NULL;
		Error("failed to init SPEEX resampler. Speex err = %d\n", err);
	}

	//Set params
	ctx->channels		= 1;
	ctx->sample_rate	= 11025;//11025;11025
	ctx->sample_fmt		= AV_SAMPLE_FMT_FLT;

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	         Error("could not open codec\n");

	//Get the number of samples
	numFrameSamples = ctx->frame_size;

	//Create frame
	frame = av_frame_alloc();
	//Set defaults
	frame->nb_samples     = ctx->frame_size;
	frame->format         = ctx->sample_fmt;
	frame->channel_layout = ctx->channel_layout;
}

NellyEncoder11Khz::~NellyEncoder11Khz()
{
	//Check
	if (ctx) {
		//Close
		avcodec_close(ctx);
		av_free(ctx);
		ctx = NULL;
	}
	if (resampler)
	{
		mcu_resampler_destroy(resampler);
		resampler = NULL;
	}
	if (frame)
		av_frame_free(&frame);
}

int NellyEncoder11Khz::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	AVPacket pkt;
	int got_output;

	SWORD buffer8[512];
	SWORD buffer11[512];
	DWORD len8 = 512;
	DWORD len11 = 512;
	float bufferf[512];

	//If we have data
	if (inLen>0)
	{
		//Push
		samples8.push(in,inLen);
		//Get length
		len8 = samples8.length();
		//Peek all
		samples8.peek(buffer8,len8);
		//Resample
		mcu_resampler_process_int(resampler,0,buffer8,&len8,buffer11,&len11);
		//Remove
		samples8.remove(len8);
		//Push
		samples11.push(buffer11,len11);
	}

	//If not enought samples
	if (samples11.length()<ctx->frame_size)
		//Exit
		return 0;
	//Get them
	samples11.pop(buffer11,ctx->frame_size);
	//For each one
	for (int i=0;i<ctx->frame_size;++i)
		//Convert to float
		bufferf[i] = buffer11[i] * (1.0 / (1<<15));
	//Fill data
	if ( avcodec_fill_audio_frame(frame, ctx->channels, ctx->sample_fmt, (BYTE*)bufferf, ctx->frame_size*sizeof(SWORD), 0)<0)
		//Exit
		return Error("NELLY: could not fill audio frame\n");

	//Reset packet
	av_init_packet(&pkt);
	
	//Set output
	pkt.data = out;
	pkt.size = outLen;

	//Encode audio
	if (avcodec_encode_audio2(ctx, &pkt, frame, &got_output)<0)
		//Exit
		return Error("NELLY: could not encode audio frame\n");

	//Check if we got output
	if (!got_output)
		//Exit
		return Error("NELLY: could not get output packet\n");

	//Return encoded size
	return pkt.size;
}

NellyDecoder11Khz::NellyDecoder11Khz()
{
	//NO ctx yet
	ctx = NULL;
	resampler = NULL;
	///Set type
	type = AudioCodec::NELLY11;

	// Get encoder
	codec = avcodec_find_decoder(AV_CODEC_ID_NELLYMOSER);

	// Check codec
	if(!codec)
		Error("No codec found\n");

	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);

	//The resampler to convert to 8Khz
	int err;
	resampler = mcu_resampler_init(1, 11025, 8000, 5, &err);
	if (err)
	{
		resampler = NULL;
		Error("failed to init SPEEX resampler. Speex err = %d\n", err);
	}

	//Set params
	ctx->request_sample_fmt		= AV_SAMPLE_FMT_S16;

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	         Error("could not open codec\n");

	//Set number of channels
	ctx->channels = 1;

	//Get the number of samples
	numFrameSamples = 160;
}

NellyDecoder11Khz::~NellyDecoder11Khz()
{
	//Check
	if (ctx) {
		//Close
		avcodec_close(ctx);
		av_free(ctx);
		ctx = NULL;
	}
	if (resampler)
	{
		mcu_resampler_destroy(resampler);
		resampler = NULL;
	}
}

int NellyDecoder11Khz::Decode(BYTE *in, int inLen, SWORD* out, int outLen)
{
	AVFrame frame;
	int got_frame;
	SWORD buffer8[512];
	SWORD buffer11[512]; 
	DWORD len8 = 512;
	
	//If we have input
	if (inLen>0)
	{
		//Create packet
		AVPacket packet;

		//Init it
		av_init_packet(&packet);

		//Set data
		packet.data = (BYTE*)in;
		packet.size = inLen;

		//Decode it
		if (avcodec_decode_audio4(ctx,&frame,&got_frame,&packet)<0)
			//nothing
			return Error("Error decoding nellymoser\n");

		//If we got a frame
		if (got_frame)
		{
			//Get data
			float *fbuffer11 = (float *) frame.extended_data[0];
			DWORD len11 = frame.nb_samples;

			//Convert to SWORD
			for (int i=0; i<len11; ++i)
				buffer11[i] = (fbuffer11[i] * (1<<15));
			
			//Resample
			mcu_resampler_process_int(resampler,0,buffer11,&len11,buffer8,&len8);

			//Append to samples
			samples.push(buffer8,len8);
		}

	}
	//Check size
	if (samples.length()<numFrameSamples)
		//Nothing yet
		return 0;

	//Pop 160
	samples.pop(out,numFrameSamples);
	//Return number of samples
	return numFrameSamples;
}

