#ifndef AACDECODER_H
#define AACDECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include "config.h"
#include <AudioBuffer.h>
#include "audio.h"
#include "aacconfig.h"

class AACDecoder : public AudioDecoder
{
public:
	AACDecoder();
	virtual ~AACDecoder();
	virtual int Decode(const AudioFrame::const_shared& frame);
	virtual AudioBuffer::shared GetDecodedAudioFrame();
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

#endif /* AACDECODER_H */

