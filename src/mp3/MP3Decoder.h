#ifndef MP3DECODER_H
#define MP3DECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include "config.h"
#include "fifo.h"
#include "audio.h"

class MP3Decoder : public AudioDecoder
{
public:
	MP3Decoder();
	virtual ~MP3Decoder();
	virtual int Decode(const BYTE *in,int inLen,SWORD* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate)	{ return ctx->sample_rate;		}
	virtual DWORD GetRate()			{ return ctx->sample_rate;		}
	virtual DWORD GetNumChannels()		{ return std::min(ctx->channels,2);	}
	bool SetConfig(const uint8_t* data,const size_t size);
private:
	bool		inited	= false;
	const AVCodec* codec	= nullptr;
	AVCodecContext*	ctx	= nullptr;
	AVPacket*	packet	= nullptr;
	AVFrame*	frame	= nullptr;

};

#endif /* MP3DECODER_H */

