extern "C" {
#include "gsm.h"
} 
#include "audio.h"

class GSMEncoder : public AudioEncoder
{
public:
	GSMEncoder(const Properties &properties);
	virtual ~GSMEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels)	{ return numChannels == 1 ? 16000 : 0; }
	virtual DWORD GetRate()			{ return 8000;	}
	virtual DWORD GetClockRate()		{ return 8000;	}
private:
	gsm g;

};


class GSMDecoder : public AudioDecoder
{
public:
	GSMDecoder();
	virtual ~GSMDecoder();
	virtual int Decode(const BYTE *in,int inLen,SWORD* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate)	{ return 8000;	}
	virtual DWORD GetRate()			{ return 8000;	}
private:
	gsm g;
};
