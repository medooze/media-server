#ifndef _AUDIOTRANSRATER_H_
#define _AUDIOTRANSRATER_H_
#include "speex/speex_resampler.h"

class AudioTransrater
{
public:
	AudioTransrater();
	~AudioTransrater();

	int Open(DWORD inputRate, DWORD outputRate, DWORD numChannels = 1);
	int ProcessBuffer(SWORD* in, DWORD sizeIn, SWORD* out, DWORD* sizeOut);
	void Close();

	bool IsOpen()	{ return resampler!=NULL; }
	
private:
	SpeexResamplerState *resampler;
};

#endif