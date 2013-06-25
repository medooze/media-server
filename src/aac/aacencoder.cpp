/* 
 * File:   aacencoder.cpp
 * Author: Sergio
 * 
 * Created on 20 de junio de 2013, 10:46
 */

#include "aacencoder.h"
#include "log.h"


AACEncoder::AACEncoder(const Properties &properties)
{
	//NO ctx yet
	ctx = NULL;
	///Set type
	type = AudioCodec::AAC;

	//Register all
	avcodec_register_all();

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);

	// Check codec
	if(!codec)
	{
		Error("AAC: No encoder found\n");
		return;
	}
	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);

	//Check
	if (ctx == NULL)
	{
		Error("AAC: could not allocate context.\n");
		return;
	}

	//Set params
	ctx->channels		= 1;
	ctx->sample_fmt		= AV_SAMPLE_FMT_FLTP;

	//Set parames
	ctx->sample_rate	= properties.GetProperty("aac.samplerate"	,8000);
	ctx->bit_rate		= properties.GetProperty("aac.bitrate"		,ctx->sample_rate*3);

	//Allow experimental codecs
	ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	{
		//Error
	        Error("AAC: could not open codec\n");
		//Exit
		return;
	}

	//Get the number of samples
	numFrameSamples = ctx->frame_size;

	//Create context for converting sampel formats
	avr = avresample_alloc_context();

	//Set options
	av_opt_set_int(avr, "in_channel_layout",  AV_CH_LAYOUT_MONO,	0);
	av_opt_set_int(avr, "out_channel_layout", AV_CH_LAYOUT_MONO,	0);
	av_opt_set_int(avr, "in_sample_rate",     ctx->sample_rate,     0);
	av_opt_set_int(avr, "out_sample_rate",    ctx->sample_rate,     0);
	av_opt_set_int(avr, "in_sample_fmt",      AV_SAMPLE_FMT_S16,   0);
	av_opt_set_int(avr, "out_sample_fmt",     AV_SAMPLE_FMT_FLTP,    0);

	//Open context
	avresample_open(avr);

	//Temporal buffer samples
	samplesNum = 2048;

	//Allocate float samples
	av_samples_alloc(&samples, &samplesSize, 1, samplesNum, AV_SAMPLE_FMT_FLTP, 0);

	//Log
	Log("AAC: Encoder open with frame size %d.\n", numFrameSamples);
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
	if(avr)
	{
		avresample_close(avr);
		avresample_free(&avr);
	}
	if (samples)
		av_freep(&samples);
}

int AACEncoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	if (!inLen)
		return 0;
	
	if (ctx == NULL)
		return Error("AAC: no context.\n");

	//If not enought samples
	if (inLen!=numFrameSamples)
		//Exit
		return Error("AAC: sample size %d is not correct. Should be %d\n", inLen, numFrameSamples);

	//Convert
	int len = avresample_convert(avr, &samples, samplesSize, samplesNum, (BYTE**)&in, inLen*sizeof(SWORD), inLen);

	//Encode
	return avcodec_encode_audio(ctx, out,outLen,(short *)samples);
} 