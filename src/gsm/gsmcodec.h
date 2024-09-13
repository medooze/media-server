extern "C" {
#include "gsm.h"
} 
#include "audio.h"

class GSMEncoder : public AudioEncoder
{
public:
	GSMEncoder(const Properties &properties);
	virtual ~GSMEncoder();
	virtual AudioFrame::shared Encode(const AudioBuffer::const_shared& audioBuffer);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels)	{ return numChannels == 1 ? 16000 : 0; }
	virtual DWORD GetRate()			{ return 8000;	}
	virtual DWORD GetClockRate()		{ return 8000;	}
private:
	gsm g;
	AudioFrame::shared audioFrame;
};


class GSMDecoder : public AudioDecoder
{
public:
	GSMDecoder();
	virtual ~GSMDecoder();
	virtual int Decode(const AudioFrame::const_shared& audioFrame);
	virtual AudioBuffer::shared GetDecodedAudioFrame();
	virtual DWORD TrySetRate(DWORD rate)	{ return 8000;	}
	virtual DWORD GetRate()			{ return 8000;	}
private:
	gsm g;
	std::pair<uint8_t*, int> audioFrameInfo;
};
