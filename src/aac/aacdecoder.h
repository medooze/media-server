#ifndef AACDECODER_H
#define AACDECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavresample/avresample.h>
#include <libavutil/opt.h>
}
#include "config.h"
#include "fifo.h"
#include "audio.h"
#include "aacconfig.h"

class AACDecoder : public AudioDecoder
{
public:
	AACDecoder();
	virtual ~AACDecoder();
	virtual int Decode(const BYTE *in,int inLen,SWORD* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate)	{ return ctx->sample_rate;	}
	virtual DWORD GetRate()			{  return ctx->sample_rate;	}
	void SetConfig(const uint8_t* data,const size_t size);
private:
	bool		inited	= false;
	AVCodec*	codec	= nullptr;
	AVCodecContext*	ctx	= nullptr;
	AVPacket*	packet	= nullptr;
	AVFrame*	frame	= nullptr;

};

#endif /* AACDECODER_H */

