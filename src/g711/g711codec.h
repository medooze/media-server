#ifndef _G711CODEC_H_
#define _G711CODEC_H_

#include "audio.h"

class PCMAEncoder : public AudioEncoder
{
public:
	PCMAEncoder(const Properties &properties);
	virtual ~PCMAEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
};

class PCMADecoder : public AudioDecoder
{
public:
	PCMADecoder();
	virtual ~PCMADecoder();
	virtual int Decode(BYTE *in,int inLen,SWORD* out,int outLen);
};

class PCMUEncoder : public AudioEncoder
{
public:
	PCMUEncoder(const Properties &properties);
	virtual ~PCMUEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
};

class PCMUDecoder : public AudioDecoder
{
public:
	PCMUDecoder();
	virtual ~PCMUDecoder();
	virtual int Decode(BYTE *in,int inLen,SWORD* out,int outLen);
};

#endif
