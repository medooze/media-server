#ifndef AUDIOBUFFER_H_
#define AUDIOBUFFER_H_
#include "config.h"
#include <memory>
#include <optional>

class AudioBuffer
{
public:
	using shared = std::shared_ptr<AudioBuffer>;
	using const_shared = std::shared_ptr<const AudioBuffer>;

	AudioBuffer(int numSamplesPerFrame, int numChannels) :
		numSamplesPerFrame(numSamplesPerFrame),
		numChannels(numChannels),
		pcmBuffer(numSamplesPerFrame*numChannels, 0)
	{

	}
	~AudioBuffer()
	{
		
	}
	QWORD	GetTimestamp() const	{ return ts; }
	void	SetTimestamp(QWORD ts)	{ this->ts = ts; }
	
	QWORD	GetClockRate() const	{ return clockRate; }
	void	SetClockRate(DWORD clockRate) { this->clockRate = clockRate; }

	const SWORD* GetData() const { return pcmBuffer.data(); }
	
	int SetSamples(SWORD* in, int numSamples)
	{
		if(!in) 
			return Error("AudioBuffer::SetSamples() empty input buffer\n");

		int totalResampled = numSamples * numChannels;
		if(totalResampled != pcmBuffer.size())
		{
			Debug("AudioBuffer::SetSamples buffer resized, resized to =%d\n", totalResampled);
			pcmBuffer.resize(totalResampled);
		}
			
		memcpy((SWORD*)pcmBuffer.data(), in, sizeof(SWORD)*totalResampled);
		return numSamples;
	}

	int SetPCMData(uint8_t** pcmData, uint32_t numSamples)
	{
		if(!pcmData || !*pcmData)
			return Error("-AudioBuffer::SetPCMData() invalid frame data pointer\n");
	
		if(numSamples*numChannels > pcmBuffer.size()) 
			return Error("-AudioBuffer::SetPCMData() exceed audio buffer size\n");
		
		int totalWrittenSamples = 0;
		for (size_t i = 0; i < numSamples; ++i)
		{
			//For each channel
			for (size_t ch = 0; ch < numChannels; ++ch)
			{
				//Interleave
				if(pcmData[ch])
					pcmBuffer[i * numChannels + ch] = ((float*)(pcmData[ch]))[i] * (1 << 15);
				else 
					return Error("-AudioBuffer::SetPCMData() invalid data pointer for ch %d\n", ch);
				totalWrittenSamples++;
			}

		}
		return totalWrittenSamples / numChannels;		
	}

	bool SetSampleAt(size_t pos, SWORD sample) 
	{ 
		if(pos >= pcmBuffer.size()) 
			return Error("-AudioBuffer::SetSampleAt() write position exceeds buffer size\n");
		pcmBuffer[pos] = sample;
		return true;
	}
	const int GetNumChannels() const { return numChannels; }
	const int GetNumSamples() const { return pcmBuffer.size() / numChannels; }

private:
	int numSamplesPerFrame;
	int numChannels;	
	QWORD ts = 0;
	DWORD clockRate = 0;
	std::vector<SWORD> pcmBuffer;
}; 

#endif // !AUDIOBUFFER_H_