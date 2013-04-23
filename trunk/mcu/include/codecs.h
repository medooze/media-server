#ifndef _CODECS_H_
#define _CODECS_H_

#include "config.h"
#include "media.h"
#include <map>

class AudioCodec
{
public:
	enum Type {PCMA=8,PCMU=0,GSM=3,SPEEX16=117,AMR=118,TELEPHONE_EVENT=100,NELLY8=130,NELLY11=131,OPUS=98};
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
			default:	return "unknown";
		}
	}
	typedef std::map<int,Type> RTPMap;
};

class VideoCodec
{
public:
	enum Type {H263_1996=34,H263_1998=103,MPEG4=104,H264=99,SORENSON=100,VP8=107,ULPFEC=108,RED=109};
	static const char* GetNameFor(Type type)
	{
		switch (type)
		{
			case H263_1996:	return "H263_1996";
			case H263_1998:	return "H263_1998";
			case MPEG4:	return "MPEG4";
			case H264:	return "H264";
			case SORENSON:  return "SORENSON";
			case VP8:	return "VP8";
			default:	return "unknown";
		}
	}
	typedef std::map<int,Type> RTPMap;
};


class TextCodec
{
public:
	enum Type {T140=106,T140RED=105};
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
	}
	return "unknown media";
}

#endif
