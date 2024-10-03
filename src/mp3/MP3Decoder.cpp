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


int MP3Decoder::Decode(const AudioFrame::const_shared& audioFrame)
{
	//Check we have config
	if (!inited)
		return Error("-MP3Decoder::Decode() Not inited\n");

	//Set data
	packet->data = audioFrame ? (uint8_t*)audioFrame->GetData() : nullptr;
	packet->size = audioFrame ? audioFrame->GetLength() : 0;
	//Decode it
	if (avcodec_send_packet(ctx, packet)<0)
		//nothing
		return Error("-AACDecoder::Decode() Error decoding AAC packet\n");
	
	//Release side data
	av_packet_free_side_data(packet);
	return 1;
}

AudioBuffer::shared MP3Decoder::GetDecodedAudioFrame()
{
	int ret;
	ret = avcodec_receive_frame(ctx, frame);
	if (ret < 0)
	{
		if(ret != AVERROR(EAGAIN))
			Error("-MP3Decoder::GetDecodedAudioFrame Error getting decoded frame reason: %d\n", ret);
		return {};
	}
	auto audioBuffer = std::make_shared<AudioBuffer>(frame->nb_samples, frame->channels);

	int len = audioBuffer->SetPCMData(frame->extended_data, frame->nb_samples);

	if(len<frame->nb_samples) 
	{
		Error("-MP3Decoder::GetDecodedAudioFrame less decoded data copied:actual=%d - should=%d\n", len, frame->nb_samples);
		return {};
	}
	return audioBuffer;
}