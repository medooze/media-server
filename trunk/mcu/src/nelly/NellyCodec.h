/* 
 * File:   NellyCodec.h
 * Author: Sergio
 *
 * Created on 7 de diciembre de 2011, 23:29
 */

#ifndef NELLYCODEC_H
#define	NELLYCODEC_H
extern "C" {
#include <libavcodec/avcodec.h>
#include <speex/speex_resampler.h>
}
#include "config.h"
#include "fifo.h"
#include "codecs.h"
#include "audio.h"

class NellyCodec : public AudioCodec
{
public:
	NellyCodec();
	virtual ~NellyCodec();
	virtual int Encode(WORD *in,int inLen,BYTE* out,int outLen);
	virtual int Decode(BYTE *in,int inLen,WORD* out,int outLen);
private:
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	fifo<WORD,512>  samples;
};

class NellyEncoder11Khz : public AudioCodec
{
public:
	NellyEncoder11Khz();
	virtual ~NellyEncoder11Khz();
	virtual int Encode(WORD *in,int inLen,BYTE* out,int outLen);
	virtual int Decode(BYTE *in,int inLen,WORD* out,int outLen);
private:
	SpeexResamplerState *resampler;
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	fifo<WORD,1024>  samples8;
	fifo<WORD,1024>  samples11;
};

class NellyDecoder11Khz : public AudioCodec
{
public:
	NellyDecoder11Khz();
	virtual ~NellyDecoder11Khz();
	virtual int Encode(WORD *in,int inLen,BYTE* out,int outLen);
	virtual int Decode(BYTE *in,int inLen,WORD* out,int outLen);
private:
	SpeexResamplerState *resampler;
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	fifo<WORD,1024>  samples;
};

#endif	/* NELLYCODEC_H */

