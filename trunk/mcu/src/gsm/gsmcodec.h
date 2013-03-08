extern "C" {
#include "gsm.h"
} 
#include "audio.h"

class GSMEncoder : public AudioEncoder
{
public:
	GSMEncoder();
	virtual ~GSMEncoder();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
private:
	gsm g;

};


class GSMDecoder : public AudioDecoder
{
public:
	GSMDecoder();
	virtual ~GSMDecoder();
	virtual int Decode(BYTE *in,int inLen,SWORD* out,int outLen);
private:
	gsm g;
};
