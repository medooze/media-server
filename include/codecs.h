#ifndef _CODECS_H_
#define _CODECS_H_

#include "config.h"
#include "media.h"
#include <map>
#include <string.h>
#include "rtp/RTPMap.h"

class AudioCodec
{
public:
	enum Type {PCMA=8,PCMU=0,GSM=3,G722=9,SPEEX16=117,AMR=118,TELEPHONE_EVENT=100,OPUS=98,AAC=97,EAC3=101,MULTIOPUS=114,MP3=33,RTX=115,UNKNOWN=-1};

public:
	static Type GetCodecForName(const char* codec)
	{
		if	(strcasecmp(codec,"PCMA")==0) return PCMA;
		else if (strcasecmp(codec,"PCMU")==0) return PCMU;
		else if (strcasecmp(codec,"GSM")==0) return GSM;
		else if (strcasecmp(codec,"SPEEX16")==0) return SPEEX16;
		else if (strcasecmp(codec,"OPUS")==0) return OPUS;
		else if (strcasecmp(codec,"MULTIOPUS")==0) return MULTIOPUS;
		else if (strcasecmp(codec,"G722")==0) return G722;
		else if (strcasecmp(codec,"AAC")==0) return AAC;
		else if (strcasecmp(codec, "EAC3") == 0) return EAC3;
		else if (strcasecmp(codec, "EC3") == 0) return EAC3;
		else if (strcasecmp(codec, "EC-3") == 0) return EAC3;
		else if (strcasecmp(codec, "MP3") == 0) return MP3;
		return UNKNOWN;
	}

	static const char* GetNameFor(Type codec)
	{
		switch (codec)
		{
			case PCMA:	return "PCMA";
			case PCMU:	return "PCMU";
			case GSM:	return "GSM";
			case SPEEX16:	return "SPEEX16";
			case OPUS:	return "OPUS";
			case MULTIOPUS:	return "MULTIOPUS";
			case G722:	return "G722";
			case AAC:	return "AAC";
			case EAC3:	return "EAC3";
			case MP3:	return "MP3";
			case RTX:	return "RTX";
			default:	return "unknown";
		}
	}

	static DWORD GetClockRate(Type codec)
	{
		switch (codec)
		{
			case PCMA:	return 8000;
			case PCMU:	return 8000;
			case GSM:	return 8000;
			case SPEEX16:	return 16000;
			case OPUS:	return 48000;
			case MULTIOPUS:	return 48000;
			case G722:	return 16000;
			case AAC:	return 90000;
			case EAC3:	return 48000;
			default:	return 8000;
		}
	}
};

class VideoCodec
{
public:
	enum Type {JPEG=16,H264=99,VP8=107,VP9=112,ULPFEC=108,FLEXFEC03=113,RED=109,RTX=110,AV1=111,H265=96,WEBP=43,UNKNOWN=-1};
	static const char* GetNameFor(Type type)
	{
		switch (type)
		{
			case JPEG:	return "JPEG";
			case H264:	return "H264";
			case H265:	return "H265";
			case VP8:	return "VP8";
			case VP9:	return "VP9";
			case AV1:	return "AV1";
			case RED:	return "RED";
			case RTX:	return "RTX";
			case ULPFEC:	return "FEC";
			case FLEXFEC03:	return "flexfec-03";
			case WEBP:	return "WEBP";
			default:	return "unknown";
		}
	}
	static Type GetCodecForName(const char* codec)
	{
		if	(strcasecmp(codec,"JPEG")==0) return JPEG;
		else if (strcasecmp(codec,"MJPEG") == 0) return JPEG;
		else if (strcasecmp(codec,"H264")==0) return H264;
		else if (strcasecmp(codec,"H265") == 0) return H265;
		else if (strcasecmp(codec,"VP8")==0) return VP8;
		else if (strcasecmp(codec,"VP9")==0) return VP9;
		else if (strcasecmp(codec,"AV1")==0) return AV1;
		else if (strcasecmp(codec,"FLEXFEC-03")==0) return FLEXFEC03;
		else if (strcasecmp(codec,"WEBP") == 0) return WEBP;
		return UNKNOWN;
	}
};


class TextCodec
{
public:
	enum Type {T140=102,T140RED=105};
	static const char* GetNameFor(Type type)
	{
		switch (type)
		{
			case T140:	return "T140";
			case T140RED:	return "T140RED";
			default:	return "unknown";
		}
	}
};

static MediaFrame::Type GetMediaForCodec(BYTE codec)
{
	switch (codec)
	{
		case AudioCodec::PCMA:
		case AudioCodec::PCMU:
		case AudioCodec::GSM:
		case AudioCodec::SPEEX16:
		case AudioCodec::OPUS:
		case AudioCodec::MULTIOPUS:
		case AudioCodec::G722:
		case AudioCodec::AAC:
		case AudioCodec::EAC3:
		case AudioCodec::MP3:
		case AudioCodec::RTX:
			return MediaFrame::Audio;
		case VideoCodec::JPEG:
		case VideoCodec::H264:
		case VideoCodec::H265:
		case VideoCodec::VP8:
		case VideoCodec::VP9:
		case VideoCodec::AV1:
		case VideoCodec::RED:
		case VideoCodec::RTX:
		case VideoCodec::ULPFEC:
		case VideoCodec::FLEXFEC03:
		case VideoCodec::WEBP:
			return MediaFrame::Video;
		case TextCodec::T140:
		case TextCodec::T140RED:
			return MediaFrame::Text;
		default:
			return MediaFrame::Unknown;
	}
}


static const char* GetNameForCodec(MediaFrame::Type media,DWORD codec)
{
	switch (media)
	{
		case MediaFrame::Audio:
			return AudioCodec::GetNameFor((AudioCodec::Type)codec);
		case MediaFrame::Video:
			return VideoCodec::GetNameFor((VideoCodec::Type)codec);
		case MediaFrame::Text:
			return TextCodec::GetNameFor((TextCodec::Type)codec);
		default:
			return "unknown codec and media";
	}
}
#endif
