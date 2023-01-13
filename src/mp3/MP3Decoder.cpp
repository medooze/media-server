extern "C" {
#include <libavcodec/avcodec.h>
}
#include "log.h"
#include "MP3Decoder.h"



MP3Decoder::MP3Decoder()
{
	//NO ctx yet
	ctx = NULL;
	///Set type
	type = AudioCodec::MP3;

	// Get encoder
	codec = avcodec_find_decoder(AV_CODEC_ID_MP3);

	// Check codec
	if(!codec)
		Error("No codec found\n");

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

MP3Decoder::~MP3Decoder()
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

bool MP3Decoder::SetConfig(const uint8_t* data,const size_t size)
{
	

	//Set side data pon packet
	uint8_t *side = av_packet_new_side_data(packet, AV_PKT_DATA_NEW_EXTRADATA, size);
	//Copy it
	memcpy(side,data,size);
	//We are inited
	inited = true;
	
	//Done
	return true;
}

int MP3Decoder::Decode(const BYTE *in, int inLen, SWORD* out, int outLen)
{
	//Check we have config
	//if (!inited)
	//	return Error("-MP3Decoder::Decode() Not inited\n");
	
	//If we have input
	if (inLen<=0)
		return 0;
	
	//Set data
	packet->data = (BYTE*)in;
	packet->size = inLen;

	//Decode it
	if (avcodec_send_packet(ctx, packet)<0)
		//nothing
		return Error("-MP3Decoder::Decode() Error decoding MP3 packet\n");
	
	//Release side data
	av_packet_free_side_data(packet);
	
	//If we got a frame
	if (avcodec_receive_frame(ctx, frame)<0)
		//Nothing yet
		return 0;
	
	//Get number of samples
	auto len = frame->nb_samples;
	//Convert to SWORD
	for (size_t i=0; i<len && (i*frame->channels)<outLen; ++i)
		//For each channel
		for (size_t n=0; n<std::min(frame->channels,2); ++n)
			//Interleave
			out[i*frame->channels + n] = ((float*)(frame->extended_data[n]))[i] * (1<<15);
	//Return number of samples
	return len;
}

