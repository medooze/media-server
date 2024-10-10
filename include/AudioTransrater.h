#ifndef _AUDIOTRANSRATER_H_
#define _AUDIOTRANSRATER_H_
#include "speex/speex_resampler.h"
#include "AudioBufferPool.h"

class AudioTransrater
{
public:
	AudioTransrater();
	~AudioTransrater();

	int Open(DWORD inputRate, DWORD outputRate, DWORD numChannels = 1);
	AudioBuffer::shared ProcessBuffer(const AudioBuffer::shared& audioBuffer);
	void Close();

	bool IsOpen()	{ return resampler!=NULL; }
	
private:
	static constexpr size_t InitialPoolSize = 30;
	static constexpr size_t MaxPoolSize = InitialPoolSize + 2;
	SpeexResamplerState *resampler;
	DWORD inputRate=0;
	DWORD outputRate=0;
	std::optional<QWORD> playPTSOffset;
	AudioBufferPool audioBufferPool;
	uint64_t scaleTimestamp(const AudioBuffer::shared& audioBuffer);
};

#endif