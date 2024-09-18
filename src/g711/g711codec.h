#ifndef _G711CODEC_H_
#define _G711CODEC_H_

#include "audio.h"
#include <queue>

class PCMAEncoder : public AudioEncoder
{
public:
	PCMAEncoder(const Properties &properties);
	virtual ~PCMAEncoder();
	virtual AudioFrame::shared Encode(const AudioBuffer::const_shared& audioBuffer);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels) { return numChannels==1 ? 8000 : 0;	}
	virtual DWORD GetRate()			{ return 8000;	}
	virtual DWORD GetClockRate()		{ return 8000;	}
private:
	AudioFrame::shared audioFrame;
};

class PCMADecoder : public AudioDecoder
{
public:
	PCMADecoder();
	virtual ~PCMADecoder();
	virtual int Decode(const AudioFrame::const_shared& frame);
	virtual AudioBuffer::shared GetDecodedAudioFrame();
	virtual DWORD TrySetRate(DWORD rate)	{ return 8000; }
	virtual DWORD GetRate()			{ return 8000;	}
private:
	std::queue<AudioBuffer::shared> audioBufferQueue;
};

class PCMUEncoder : public AudioEncoder
{
public:
	PCMUEncoder(const Properties &properties);
	virtual ~PCMUEncoder();
	virtual AudioFrame::shared Encode(const AudioBuffer::const_shared& audioBuffer);
	virtual DWORD TrySetRate(DWORD rate, DWORD numChannels) { return numChannels == 1 ? 8000 : 0; }
	virtual DWORD GetRate()			{ return 8000;	}
	virtual DWORD GetClockRate()		{ return 8000;	}
private:
	AudioFrame::shared audioFrame;
};

class PCMUDecoder : public AudioDecoder
{
public:
	PCMUDecoder();
	virtual ~PCMUDecoder();
	virtual int Decode(const AudioFrame::const_shared& frame);
	virtual AudioBuffer::shared GetDecodedAudioFrame();
	virtual DWORD TrySetRate(DWORD rate)	{ return 8000;	}
	virtual DWORD GetRate()			{ return 8000;	}
private:
	std::queue<AudioBuffer::shared> audioBufferQueue;
};

#endif
