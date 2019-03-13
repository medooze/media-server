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
		Error("No codec found\n");

	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);

	//Set params
	ctx->request_sample_fmt		= AV_SAMPLE_FMT_S16;

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

void AACDecoder::SetConfig(const uint8_t* data,const size_t size)
{
	AACSpecificConfig config;
	
	Debug("-AACDecoder::SetConfig()\n");
	
	//Decode it
	if (config.Decode(data,size))
	{
		//Set data
		ctx->channels		= config.GetChannels();
		ctx->sample_rate	= config.GetRate();
		numFrameSamples		= config.GetFrameLength();
	}

	//Set side data pon packet
	uint8_t *side = av_packet_new_side_data(packet, AV_PKT_DATA_NEW_EXTRADATA, size);
	//Copy it
	memcpy(side,data,size);
	//We are inited
	inited = true;
}

int AACDecoder::Decode(BYTE *in, int inLen, SWORD* out, int outLen)
{
	//Check we have config
	if (!inited)
		return Error("Not inited\n");
	
	//If we have input
	if (inLen<=0)
		return 0;
	
	//Set data
	packet->data = (BYTE*)in;
	packet->size = inLen;

	//Decode it
	if (avcodec_send_packet(ctx, packet)<0)
		//nothing
		return Error("Error decoding AAC\n");
	
	//Release side data
	av_packet_free_side_data(packet);
	
	//If we got a frame
	if (avcodec_receive_frame(ctx, frame)<0)
		//Nothing yet
		return 0;
	
	//Get data
	float *buffer = (float *) frame->extended_data[0];
	auto len = frame->nb_samples;

	//Convert to SWORD
	for (size_t i=0; i<len && i<outLen; ++i)
		out[i] = (buffer[i] * (1<<15));

	//Return number of samples
	return len;
}

