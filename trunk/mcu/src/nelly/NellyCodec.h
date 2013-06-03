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

class NellyEncoder : public AudioEncoder
{
public:
	NellyEncoder(const Properties &properties);
	virtual ~NellyEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
private:
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	fifo<SWORD,512>  samples;
};

class NellyEncoder11Khz : public AudioEncoder
{
public:
	NellyEncoder11Khz(const Properties &properties);
	virtual ~NellyEncoder11Khz();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
private:
	SpeexResamplerState *resampler;
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	fifo<SWORD,1024>  samples8;
	fifo<SWORD,1024>  samples11;
};

class NellyDecoder11Khz : public AudioDecoder
{
public:
	NellyDecoder11Khz();
	virtual ~NellyDecoder11Khz();
	virtual int Decode(BYTE *in,int inLen,SWORD* out,int outLen);
private:
	SpeexResamplerState *resampler;
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	fifo<SWORD,1024>  samples;
};

#endif	/* NELLYCODEC_H */

