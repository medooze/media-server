extern "C" {
#include <speex/speex.h>
}

#include "audio.h"

class SpeexEncoder : public AudioEncoder
{
public:
	SpeexEncoder(const Properties &properties);
	virtual ~SpeexEncoder();
	virtual AudioFrame* Encode(const AudioBuffer::const_shared& audioBuffer);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels);
	virtual DWORD GetRate();
	virtual DWORD GetClockRate()	{ return 16000;}
private:
	void *encoder;
	SpeexBits encbits;
	AudioFrame audioFrame;
};

class SpeexDecoder : public AudioDecoder
{
public:
	SpeexDecoder();
	virtual ~SpeexDecoder();
	virtual int Decode(const AudioFrame::const_shared& audioFrame);
	virtual AudioBuffer::shared GetDecodedAudioFrame();
	virtual DWORD TrySetRate(DWORD rate);
	virtual DWORD GetRate()		{ return 16000;}
private:
	void *decoder;
	SpeexBits decbits;
	int dec_frame_size;

};
