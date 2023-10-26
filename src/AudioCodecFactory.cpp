#include "log.h"
#include "AudioCodecFactory.h"
#include "g711/g711codec.h"
#include "gsm/gsmcodec.h"
#include "speex/speexcodec.h"
#include "opus/opusencoder.h"
#include "opus/opusdecoder.h"
#include "g722/g722codec.h"
#include "aac/aacencoder.h"
#include "aac/aacdecoder.h"
#include "mp3/MP3Decoder.h"


AudioEncoder* AudioCodecFactory::CreateEncoder(AudioCodec::Type codec)
{
	//Empty properties
	Properties properties;

	//Create codec
	return CreateEncoder(codec,properties);
}

AudioEncoder* AudioCodecFactory::CreateEncoder(AudioCodec::Type codec, const Properties &properties)
{
	Log("-CreateAudioEncoder [%d,%s]\n",codec,AudioCodec::GetNameFor(codec));

	//Creamos uno dependiendo del tipo
	switch(codec)
	{
		case AudioCodec::GSM:
			return new GSMEncoder(properties);
		case AudioCodec::PCMA:
			return new PCMAEncoder(properties);
		case AudioCodec::PCMU:
			return new PCMUEncoder(properties);
		case AudioCodec::SPEEX16:
			return new SpeexEncoder(properties);
		case AudioCodec::OPUS:
			return new OpusEncoder(properties);
		case AudioCodec::G722:
			return new G722Encoder(properties);
		case AudioCodec::AAC:
			return new AACEncoder(properties);
		default:
			Error("Codec not found [%d]\n",codec);
	}

	return NULL;
}

AudioDecoder* AudioCodecFactory::CreateDecoder(AudioCodec::Type codec)
{
	Log("-CreateAudioDecoder [%d,%s]\n",codec,AudioCodec::GetNameFor(codec));

	//Creamos uno dependiendo del tipo
	switch(codec)
	{
		case AudioCodec::GSM:
			return new GSMDecoder();
		case AudioCodec::PCMA:
			return new PCMADecoder();
		case AudioCodec::PCMU:
			return new PCMUDecoder();
		case AudioCodec::SPEEX16:
			return new SpeexDecoder();
		case AudioCodec::OPUS:
			return new OpusDecoder();
		case AudioCodec::G722:
			return new G722Decoder();
		case AudioCodec::AAC:
			return new AACDecoder();
		case AudioCodec::MP3:
			return new MP3Decoder();
		default:
			Error("Codec not found [%d]\n",codec);
	}

	return NULL;
}
