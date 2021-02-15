#ifndef G722_H
#define	G722_H
extern "C" {
#include "g722_enc_dec.h"
}
#include "config.h"
#include "fifo.h"
#include "codecs.h"
#include "audio.h"

class G722Encoder : public AudioEncoder
{
public:
	G722Encoder(const Properties &properties);
	virtual ~G722Encoder() = default;
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels)	{ return numChannels == 1 ? 16000 : 0; }
	virtual DWORD GetRate()			{ return 16000;	}
	virtual DWORD GetClockRate()		{ return 8000;	}
private:
	G722EncoderState encoder = {};
};

class G722Decoder : public AudioDecoder
{
public:
	G722Decoder();
	virtual ~G722Decoder() = default;
	virtual int Decode(const BYTE *in,int inLen,SWORD* out,int outLen);
	virtual DWORD TrySetRate(DWORD rate)	{ return 16000;	}
	virtual DWORD GetRate()			{ return 16000;	}
private:
	G722DecoderState decoder = {};
};

#endif	/* NELLYCODEC_H */

