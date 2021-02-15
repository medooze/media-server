#ifndef _G711CODEC_H_
#define _G711CODEC_H_

#include "audio.h"

class PCMAEncoder : public AudioEncoder
{
public:
	PCMAEncoder(const Properties &properties);
	virtual ~PCMAEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels) { return numChannels==1 ? 8000 : 0;	}
	virtual DWORD GetRate()			{ return 8000;	}
	virtual DWORD GetClockRate()		{ return 8000;	}

};

class PCMADecoder : public AudioDecoder
{
public:
	PCMADecoder();
	virtual ~PCMADecoder();
	virtual int Decode(const BYTE *in,int inLen,SWORD* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate)	{ return 8000; }
	virtual DWORD GetRate()			{ return 8000;	}
};

class PCMUEncoder : public AudioEncoder
{
public:
	PCMUEncoder(const Properties &properties);
	virtual ~PCMUEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels) { return numChannels == 1 ? 8000 : 0; }
	virtual DWORD GetRate()			{ return 8000;	}
	virtual DWORD GetClockRate()		{ return 8000;	}
};

class PCMUDecoder : public AudioDecoder
{
public:
	PCMUDecoder();
	virtual ~PCMUDecoder();
	virtual int Decode(const BYTE *in,int inLen,SWORD* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate)	{ return 8000;	}
	virtual DWORD GetRate()			{ return 8000;	}
};

#endif
