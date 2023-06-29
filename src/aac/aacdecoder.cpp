extern "C" {
#include <libavcodec/avcodec.h>
}
#include "log.h"
#include "aacdecoder.h"
#include "aacconfig.h"



AACDecoder::AACDecoder()
{
	//NO ctx yet
	ctx = NULL;
	///Set type
	type = AudioCodec::AAC;

	// Get encoder
	codec = avcodec_find_decoder(AV_CODEC_ID_AAC);

	// Check codec
	if(!codec)
	{
		Error("No codec found\n");
		return;
	}

	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);

	//Set params
	ctx->request_sample_fmt		= AV_SAMPLE_FMT_FLTP;

	//OPEN it
	if (avcodec_open2(ctx, codec, NULL) < 0)
	         Error("could not open codec\n");
	
	//Create packet
	packet = av_packet_alloc();
	//Allocate frame
	frame = av_frame_alloc();
	
	//Get the number of samples
	numFrameSamples = 1024;
}

AACDecoder::~AACDecoder()
{
	//Check
	if (ctx) 
	{
		//Close
		avcodec_close(ctx);
		av_free(ctx);
	}
	
	if (packet)
		//Release pacekt
		av_packet_free(&packet);
	if (frame)
		//Release frame
		av_frame_free(&frame);
}

bool AACDecoder::SetConfig(const uint8_t* data,const size_t size)
{
	AACSpecificConfig config;
	
	Debug("-AACDecoder::SetConfig()\n");
	
	//Decode it
	if (!config.Decode(data,size))
		//Error
		return false;
	
	//Set data
	ctx->channels		= config.GetChannels();
	ctx->sample_rate	= config.GetRate();
	numFrameSamples		= config.GetFrameLength();

	//Set side data pon packet
	uint8_t *side = av_packet_new_side_data(packet, AV_PKT_DATA_NEW_EXTRADATA, size);
	//Copy it
	memcpy(side,data,size);
	//We are inited
	inited = true;
	
	//Done
	return true;
}

int AACDecoder::Decode(const BYTE *in, int inLen, SWORD* out, int outLen)
{
	//Check we have config
	if (!inited)
		return Error("-AACDecoder::Decode() Not inited\n");
	
	//If we have input
	if (inLen<=0)
		return 0;
	
	//Set data
	packet->data = (BYTE*)in;
	packet->size = inLen;

	//Decode it
	if (avcodec_send_packet(ctx, packet)<0)
		//nothing
		return Error("-AACDecoder::Decode() Error decoding AAC packet\n");
	
	//Release side data
	av_packet_free_side_data(packet);

	//Copy outout data
	int len = 0;

	//While we got decoded frames
	while (avcodec_receive_frame(ctx, frame) >= 0)
	{
		//Make sure that we have enough data
		if (outLen < (len + frame->nb_samples) * frame->channels)
			return Error("-AACDecoder::Decode() | AAC returned too much data [len:%d,samples:%d,channels:%d,outLen:%d]\n", len, frame->nb_samples, frame->channels, outLen);

		//Convert to SWORD
		for (size_t i = 0; i < frame->nb_samples && (i * frame->channels) < outLen; ++i)
			//For each channel
			for (size_t n = 0; n < std::min(frame->channels, 2); ++n)
				//Interleave
				out[(len + i) * frame->channels + n] = ((float*)(frame->extended_data[n]))[i] * (1 << 15);
		//Get number of samples
		len += frame->nb_samples;
	}

	//Return number of samples
	return len;
}

