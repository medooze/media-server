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
		pcmBuffer(numSamplesPerFrame*numChannels, 0),
		data(pcmBuffer.data()),
		left(pcmBuffer.size())
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
	SWORD* ConsumeData(int numSamples) 
	{ 
		if(numSamples > left) return nullptr;
		left -= numSamples;
		auto currData = data;
		data += numSamples;
		return currData;
	}
	int CopyAudioBufferData(const AudioBuffer::const_shared& audioBuffer, int numSamples)
	{
		SWORD* in = const_cast<SWORD*>(audioBuffer->GetData());
		if(!in) 
			return Error("AudioBuffer::CopyAudioBufferData() empty input buffer\n");
		int totalSamples = numChannels * numSamples;
		if(totalSamples > pcmBuffer.capacity())
			return Error("AudioBuffer::CopyAudioBufferData() exceed audio buffer capacity, want=%d, capacity=%d\n", totalSamples, pcmBuffer.capacity());
		memcpy(this->ConsumeData(totalSamples), in, totalSamples*sizeof(SWORD));
		return totalSamples;
	}

	int CopyResampled(SWORD* in, int numSamples)
	{
		if(!in) 
			return Error("AudioBuffer::CopyResampled() empty input buffer\n");

		int totalResampled = numSamples * numChannels;
		if(totalResampled != pcmBuffer.size())
		{
			Debug("AudioBuffer::CopyResampled buffer resized, resized to =%d\n", totalResampled);
			pcmBuffer.resize(totalResampled);
		}
			
		memcpy((SWORD*)pcmBuffer.data(), in, sizeof(SWORD)*totalResampled);
		return numSamples;
	}

	int CopyDecodedData(uint8_t** pcmData, uint32_t numSamples)
	{
		if(!pcmData || !*pcmData)
			return Error("-AudioBuffer::CopyDecodedData() invalid frame data pointer\n");
	
		if(numSamples*numChannels > pcmBuffer.size()) 
			return Error("-AudioBuffer::CopyDecodedData() exceed audio buffer size\n");
		
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
					return Error("-AudioBuffer::CopyDecodedData() invalid data pointer for ch %d\n", ch);
				totalWrittenSamples++;
			}

		}
		return totalWrittenSamples / numChannels;		
	}
	bool AddPCM(SWORD val, int pos) 
	{ 
		if(pos >= pcmBuffer.size()) 
			return Error("-AudioBuffer::AddPCM() write position exceeds buffer size\n");
		pcmBuffer[pos] = val;
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
	SWORD* data;
	int left;
}; 

#endif // !AUDIOBUFFER_H_