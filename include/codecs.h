#ifndef _CODECS_H_
#define _CODECS_H_

#include "config.h"
#include "media.h"
#include <map>
#include <string.h>

class AudioCodec
{
public:
	enum Type {PCMA=8,PCMU=0,GSM=3,G722=9,SPEEX16=117,AMR=118,TELEPHONE_EVENT=100,NELLY8=130,NELLY11=131,OPUS=98,AAC=97,MULTIOPUS=114,UNKNOWN=-1};

public:
	static Type GetCodecForName(const char* codec)
	{
		if	(strcasecmp(codec,"PCMA")==0) return PCMA;
		else if (strcasecmp(codec,"PCMU")==0) return PCMU;
		else if (strcasecmp(codec,"GSM")==0) return GSM;
		else if (strcasecmp(codec,"SPEEX16")==0) return SPEEX16;
		else if (strcasecmp(codec,"NELLY8")==0) return NELLY8;
		else if (strcasecmp(codec,"NELLY11")==0) return NELLY11;
		else if (strcasecmp(codec,"OPUS")==0) return OPUS;
		else if (strcasecmp(codec,"MULTIOPUS")==0) return MULTIOPUS;
		else if (strcasecmp(codec,"G722")==0) return G722;
		else if (strcasecmp(codec,"AAC")==0) return AAC;
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
			case NELLY8:	return "NELLY8Khz";
			case NELLY11:	return "NELLY11Khz";
			case OPUS:	return "OPUS";
			case MULTIOPUS:	return "MULTIOPUS";
			case G722:	return "G722";
			case AAC:	return "AAC";
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
			case NELLY8:	return 8000;
			case NELLY11:	return 11000;
			case OPUS:	return 48000;
			case MULTIOPUS:	return 48000;
			case G722:	return 16000;
			case AAC:	return 90000;
			default:	return 8000;
		}
	}

	typedef std::map<int,Type> RTPMap;
};

class VideoCodec
{
public:
	enum Type {H263_1996=34,H263_1998=103,MPEG4=104,H264=99,SORENSON=100,VP6=106,VP8=107,VP9=112,ULPFEC=108,FLEXFEC=113,RED=109,RTX=110,AV1=111,UNKNOWN=-1};
	static const char* GetNameFor(Type type)
	{
		switch (type)
		{
			case H263_1996:	return "H263_1996";
			case H263_1998:	return "H263_1998";
			case MPEG4:	return "MPEG4";
			case H264:	return "H264";
			case SORENSON:  return "SORENSON";
			case VP6:	return "VP6";
			case VP8:	return "VP8";
			case VP9:	return "VP9";
			case AV1:	return "AV1";
			case RED:	return "RED";
			case RTX:	return "RTX";
			case ULPFEC:	return "FEC";
			case FLEXFEC:	return "flexfec-03";
			default:	return "unknown";
		}
	}
	static Type GetCodecForName(const char* codec)
	{
		if	(strcasecmp(codec,"H263_1996")==0) return H263_1996;
		else if (strcasecmp(codec,"H263-1996")==0) return H263_1996;
		else if (strcasecmp(codec,"H263")==0) return H263_1996;
		else if (strcasecmp(codec,"H263P")==0) return H263_1998;
		else if (strcasecmp(codec,"H263_1998")==0) return H263_1998;
		else if (strcasecmp(codec,"H263-1998")==0) return H263_1998;
		else if (strcasecmp(codec,"MPEG4")==0) return MPEG4;
		else if (strcasecmp(codec,"H264")==0) return H264;
		else if (strcasecmp(codec,"SORENSON")==0) return SORENSON;
		else if (strcasecmp(codec,"VP6")==0) return VP6;
		else if (strcasecmp(codec,"VP8")==0) return VP8;
		else if (strcasecmp(codec,"VP9")==0) return VP9;
		else if (strcasecmp(codec,"AV1")==0) return AV1;
		else if (strcasecmp(codec,"FLEXFEC")==0) return FLEXFEC;
		return UNKNOWN;
	}
	typedef std::map<int,Type> RTPMap;
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
	typedef std::map<int,Type> RTPMap;
};

static MediaFrame::Type GetMediaForCodec(BYTE codec)
{
	switch (codec)
	{
		case AudioCodec::PCMA:
		case AudioCodec::PCMU:
		case AudioCodec::GSM:
		case AudioCodec::SPEEX16:
		case AudioCodec::NELLY8:
		case AudioCodec::NELLY11:
		case AudioCodec::OPUS:
		case AudioCodec::MULTIOPUS:
		case AudioCodec::G722:
		case AudioCodec::AAC:
			return MediaFrame::Audio;
		case VideoCodec::H263_1996:
		case VideoCodec::H263_1998:
		case VideoCodec::MPEG4:
		case VideoCodec::H264:
		case VideoCodec::SORENSON:  
		case VideoCodec::VP6:
		case VideoCodec::VP8:
		case VideoCodec::VP9:
		case VideoCodec::AV1:
		case VideoCodec::RED:
		case VideoCodec::RTX:
		case VideoCodec::ULPFEC:
		case VideoCodec::FLEXFEC:
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
