extern "C" {
#include <speex/speex.h>
#include <speex/speex_resampler.h>
}

#include "codecs.h"

class SpeexCodec : public AudioCodec
{
public:
	SpeexCodec();
	virtual ~SpeexCodec();
	virtual int Encode(SWORD *in,int inLen,BYTE* out,int outLen);
	virtual int Decode(BYTE *in,int inLen,SWORD* out,int outLen);
private:
	void *encoder;
	void *decoder;
	SpeexResamplerState *resampler;
	SpeexResamplerState *wbresampler;
	SpeexBits decbits;
	SpeexBits encbits;
	int dec_frame_size, enc_frame_size;

};
