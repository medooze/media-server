extern "C" {
#include <libavcodec/avcodec.h>
}
#include "log.h"
#include "AACDecoder.h"
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
	if (inited) return true;
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

int AACDecoder::Decode(const AudioFrame::const_shared& audioFrame)
{
	//Check we have config
	if (audioFrame->HasCodecConfig())
		SetConfig(audioFrame->GetCodecConfigData(), audioFrame->GetCodecConfigSize());

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

AudioBuffer::shared AACDecoder::GetDecodedAudioFrame()
{
	int ret;
	ret = avcodec_receive_frame(ctx, frame);
	if (ret < 0)
	{
		if(ret != AVERROR(EAGAIN))
			Error("-AACDecoder::GetDecodedAudioFrame Error getting decoded frame reason: %d\n", ret);
		return {};
	}
	auto audioBuffer = std::make_shared<AudioBuffer>(frame->nb_samples, frame->channels);

	int len = audioBuffer->SetPCMData(frame->extended_data, frame->nb_samples);

	if(len<frame->nb_samples) 
		Error("-AACDecoder::GetDecodedAudioFrame less decoded data copied:actual=%d - should=%d\n", len, frame->nb_samples);

	return audioBuffer;
}

