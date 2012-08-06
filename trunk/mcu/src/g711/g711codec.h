#ifndef _G711CODEC_H_
#define _G711CODEC_H_

#include "codecs.h"

class PCMACodec : public AudioCodec
{
public:
	PCMACodec();
	virtual ~PCMACodec();
	virtual int Encode(WORD *in,int inLen,BYTE* out,int outLen);
	virtual int Decode(BYTE *in,int inLen,WORD* out,int outLen);

};

class PCMUCodec : public AudioCodec
{
public:
	PCMUCodec();
	virtual ~PCMUCodec();
	virtual int Encode(WORD *in,int inLen,BYTE* out,int outLen);
	virtual int Decode(BYTE *in,int inLen,WORD* out,int outLen);

};

#endif
