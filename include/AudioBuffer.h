#ifndef AUDIOBUFFER_H_
#define AUDIOBUFFER_H_
#include "config.h"
#include "log.h"
#include <memory>
#include <optional>

class AudioBuffer
{
public:
	using shared = std::shared_ptr<AudioBuffer>;
	using const_shared = std::shared_ptr<const AudioBuffer>;

	AudioBuffer(uint16_t numSamplesPerFrame, uint8_t numChannels) :
		numSamplesPerFrame(numSamplesPerFrame),
		numChannels(numChannels),
		pcmBuffer(numSamplesPerFrame* numChannels, 0)
	{
	}

	uint64_t GetTimestamp() const { return ts; }
	void	 SetTimestamp(QWORD ts) { this->ts = ts; }

	uint64_t GetClockRate() const { return clockRate; }
	void	 SetClockRate(DWORD clockRate) { this->clockRate = clockRate; }

	const int16_t* GetData() const { return pcmBuffer.data(); }
	uint16_t SetSamples(int16_t* in, uint16_t numSamples, size_t offset=0, bool allowResize=false)
	{
		if (!in)
			return Error("AudioBuffer::SetSamples() empty input buffer\n");

		uint16_t totalSamples = numSamples * numChannels;
		if (allowResize && totalSamples != pcmBuffer.size())
		{
			Debug("AudioBuffer::SetSamples buffer resized, resized to =%d\n", totalSamples);
			pcmBuffer.resize(totalSamples);
		}
		else if (!allowResize && pcmBuffer.size() < offset+totalSamples)
			return Error("AudioBuffer::SetSamples() not enough spaces\n");
		
		memcpy((int16_t*)pcmBuffer.data()+offset, in, sizeof(int16_t) * totalSamples);
		return numSamples;
	}

	uint16_t SetPCMData(uint8_t** pcmData, uint16_t numSamples)
	{
		if (!pcmData || !*pcmData)
			return Error("-AudioBuffer::SetPCMData() invalid frame data pointer\n");

		if (numSamples * numChannels > pcmBuffer.size())
			return Error("-AudioBuffer::SetPCMData() exceed audio buffer size\n");

		uint16_t totalWrittenSamples = 0;
		for (size_t i = 0; i < numSamples; ++i)
		{
			//For each channel
			for (size_t ch = 0; ch < numChannels; ++ch)
			{
				//Interleave
				if (pcmData[ch])
					pcmBuffer[i * numChannels + ch] = ((float*)(pcmData[ch]))[i] * (1 << 15);
				else
					return Error("-AudioBuffer::SetPCMData() invalid data pointer for ch %d\n", ch);
				totalWrittenSamples++;
			}

		}
		return totalWrittenSamples / numChannels;
	}

	bool SetSampleAt(size_t pos, int16_t sample)
	{
		if (pos >= pcmBuffer.size())
			return Error("-AudioBuffer::SetSampleAt() write position exceeds buffer size\n");
		pcmBuffer[pos] = sample;
		return true;
	}
	uint8_t  GetNumChannels() const { return numChannels; }
	uint16_t GetNumSamples() const { return pcmBuffer.size() / numChannels; }

private:
	uint16_t numSamplesPerFrame;
	uint8_t  numChannels;
	uint64_t ts = 0;
	uint16_t clockRate = 0;
	std::vector<int16_t> pcmBuffer;
};

#endif // !AUDIOBUFFER_H_