#include "log.h"
#include "codecs.h"
#include "g711/g711codec.h"
#include "gsm/gsmcodec.h"
#include "speex/speexcodec.h"
#include "nelly/NellyCodec.h"


const char* AudioCodec::GetNameFor(Type codec)
{
	switch (codec)
	{
		case PCMA:	return "PCMA";
		case PCMU:	return "PCMU";
		case GSM:	return "GSM";
		case SPEEX16:	return "SPEEX16";
		case NELLY8:	return "NELLY8Khz";
		case NELLY11:	return "NELLY11Khz";
		default:	return "unknown";
	}
}

AudioCodec* AudioCodec::CreateCodec(Type codec)
{
	Log("-CreateAudioCodec [%d,%s]\n",codec,GetNameFor(codec));

	//Creamos uno dependiendo del tipo
	switch(codec)
	{
		case AudioCodec::GSM:
			return new GSMCodec();
		case AudioCodec::PCMA:
			return new PCMACodec();
		case AudioCodec::PCMU:
			return new PCMUCodec();
		case AudioCodec::SPEEX16:
			return new SpeexCodec();
		case AudioCodec::NELLY8:
			return new NellyCodec();
		case AudioCodec::NELLY11:
			return new NellyEncoder11Khz();
		default:
			Error("Codec not found [%d]\n",codec);
	}

	return NULL;
}

AudioCodec* AudioCodec::CreateDecoder(Type codec)
{
	Log("-CreateAudioCodec [%d,%s]\n",codec,GetNameFor(codec));

	//Creamos uno dependiendo del tipo
	switch(codec)
	{
		case AudioCodec::GSM:
			return new GSMCodec();
		case AudioCodec::PCMA:
			return new PCMACodec();
		case AudioCodec::PCMU:
			return new PCMUCodec();
		case AudioCodec::SPEEX16:
			return new SpeexCodec();
		case AudioCodec::NELLY8:
			return new NellyCodec();
		case AudioCodec::NELLY11:
			return new NellyDecoder11Khz();
		default:
			Error("Codec not found [%d]\n",codec);
	}

	return NULL;
}
