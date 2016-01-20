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
#include "g722codec.h"
#include "fifo.h"
#include "log.h"

G722Encoder::G722Encoder(const Properties &properties)
{
	//NO ctx yet
	ctx = NULL;
	frame = NULL;
	///Set type
	type = AudioCodec::G722;

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_ADPCM_G722);

	// Check codec
	if(!codec)
	{
		Error("G722: No encoder found\n");
		return;
	}
	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);
	if (ctx == NULL)
	{
		Error("G722: could not allocate context.\n");
		return;
	}
	//Set params
	ctx->channels		= 1;
	ctx->sample_rate	= 16000;
	ctx->sample_fmt		= AV_SAMPLE_FMT_S16;
	

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	{
		av_free(ctx);
		ctx = NULL;
	        Error("G722: could not open codec\n");
	}
	else
	{
		//Get the number of samples
		if ( ctx->frame_size > 0 ) 
			numFrameSamples = ctx->frame_size;
		else
		{
			numFrameSamples = 20 * 16; // 20 ms @ 16 Khz = 320 samples
			ctx->frame_size = numFrameSamples;
		}
		//Create frame
		frame = av_frame_alloc();
		//Set defaults
		frame->nb_samples     = ctx->frame_size;
    		frame->format         = ctx->sample_fmt;
    		frame->channel_layout = ctx->channel_layout;

		Log("G722: Encoder open with frame size %d.\n", numFrameSamples);
	}
}

G722Encoder::~G722Encoder()
{
	//Check
	if (ctx)
	{
		//Close
		avcodec_close(ctx);
		av_free(ctx);
	}
	if (frame)
		  av_frame_free(&frame);
}

int G722Encoder::Encode (SWORD *in,int inLen,BYTE* out,int outLen)
{
	AVPacket pkt;
	int got_output;

	if (ctx == NULL)
		return Error("G722: no context.\n");

	//If not enought samples
	if (inLen <= 0)
		//Exit
		return Error("G722: sample size %d is not correct. Should be %d\n", inLen, numFrameSamples);

	//Fill data
	if ( avcodec_fill_audio_frame(frame, ctx->channels, ctx->sample_fmt, (BYTE*)in, inLen*sizeof(SWORD), 0)<0)
		//Exit
		return Error("G722: could not fill audio frame\n");

	//Reset packet
	av_init_packet(&pkt);


	//Set output
	pkt.data = out;
	pkt.size = outLen;

	//Encode audio
	if (avcodec_encode_audio2(ctx, &pkt, frame, &got_output)<0)
		//Exit
		return Error("G722: could not encode audio frame\n");

	//Check if we got output
	if (!got_output)
		//Exit
		return Error("G722: could not get output packet\n");
		
	//Return encoded size
	return pkt.size;
}

G722Decoder::G722Decoder()
{
	//NO ctx yet
	ctx = NULL;
	///Set type
	type = AudioCodec::G722;

	// Get encoder
	codec = avcodec_find_decoder(AV_CODEC_ID_ADPCM_G722);

	// Check codec
	if(!codec)
		Error("G722: No decoder found\n");

	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);
	frame = av_frame_alloc();

	//The resampler to convert to 8Khz
	int err;
	//Set params
	ctx->request_sample_fmt		= AV_SAMPLE_FMT_S16;
	//Set number of channels
	ctx->channels = 1;

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	{
		av_free(ctx);
		ctx = NULL;
	         Error("ffmpeg: could not open codec G.722\n");
	}
	else
	{
               if ( ctx->frame_size > 0 )
                        numFrameSamples = ctx->frame_size;
                else
                        numFrameSamples = 20 * 16; // 20 ms @ 16 Khz = 320 samples
                Log("G722: Decoder open with frame size %d.\n", numFrameSamples);
	}
}

G722Decoder::~G722Decoder()
{
	//Check
	if (ctx)
	{
		//Close
		avcodec_close(ctx);
		av_free(ctx);
	}
	
	if (frame)
		av_free(frame);
}

int G722Decoder::Decode(BYTE *in, int inLen, SWORD* out, int outLen)
{
	int got_frame;
	
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
		if (avcodec_decode_audio4(ctx,frame,&got_frame,&packet)<0)
			//nothing
			return Error("Error decoding G.722\n");

		//If we got a frame
		if (got_frame)
		{
			//Get data
			SWORD *buffer16 = (SWORD *)frame->extended_data[0];
			DWORD len16 = frame->nb_samples;
			//Push decoded
			samples.push(buffer16,len16);
		}
		
		//Release frame
		av_frame_unref(frame);
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

