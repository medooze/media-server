#include "log.h"
#include "AudioTransrater.h"
#include <cmath>

AudioTransrater::AudioTransrater():
	audioBufferPool(InitialPoolSize, MaxPoolSize)
{
	//No resampler
	resampler = NULL;
}

AudioTransrater::~AudioTransrater()
{
	//Close
	Close();
}

int AudioTransrater::Open(DWORD inputRate, DWORD outputRate, DWORD numChannels)
{
	int err;

	//Close it before
	Close();

	//Check rates
	if (!inputRate || !outputRate)
		//Exit
		return Error("-Sample rates not correct [in:%d,out:%d]\n",inputRate,outputRate);

	//Check if both are equal
	if (inputRate==outputRate)
		//Exit
		return Log("-No resampling needed, same sample rate [in:%d,out:%d]\n",inputRate,outputRate);

	//Create resampler
	resampler = mcu_resampler_init(numChannels, inputRate, outputRate, 10, &err);

	//Check error
	if (err)
	{
		//Nullify
		resampler = NULL;
		//Exit
		return Error("-AudioTransrater: failed to init SPEEX resampler. Speex err = %d\n", err);
	}
	this->inputRate = inputRate;
	this->outputRate = outputRate;

	//OK
	return 1;
}

void AudioTransrater::Close()
{
	//Check if opened
	if (resampler)
	{
		//Destroy resampler
		mcu_resampler_destroy(resampler);
		//Nullify
		resampler = NULL;
	}
}

AudioBuffer::shared AudioTransrater::ProcessBuffer(const AudioBuffer::shared& audioBuffer)
{
	auto in = audioBuffer->GetData();
	auto sizeIn = static_cast<uint32_t>(audioBuffer->GetNumSamples()/audioBuffer->GetNumChannels());

	auto bufferSize = static_cast<uint32_t>(std::ceil(static_cast<double>(outputRate) / inputRate * sizeIn));	
	audioBufferPool.SetSize(bufferSize, audioBuffer->GetNumChannels());
	auto resampledBuffer = audioBufferPool.Acquire();
	auto resampleSize = bufferSize;
	auto out = resampledBuffer->GetData();

	int err = mcu_resampler_process_interleaved_int(resampler, (spx_int16_t*)in, (spx_uint32_t*)&sizeIn, (spx_int16_t*)out, (spx_uint32_t*)&resampleSize);
	//Check error
	if (err)
	{
		Error("-AudioTransrater: resampling error. ErrCode = %d.\n", err);
		return {};
	}

	if (resampleSize != bufferSize)
		if (!resampledBuffer->Resize(resampleSize))
			return {};
	
	resampledBuffer->SetTimestamp(scaleTimestamp(audioBuffer));
	resampledBuffer->SetClockRate(outputRate);
	//OK
	return resampledBuffer;
}

uint64_t AudioTransrater::scaleTimestamp(const AudioBuffer::shared& audioBuffer)
{
	if (!playPTSOffset.has_value())
		playPTSOffset = audioBuffer->GetTimestamp();
	auto ptsDiff = audioBuffer->GetTimestamp() - playPTSOffset.value();
	// scale the pts based on recordRate which is the clockrate for encoded audio
	// so that the pts stored in audio buffer is always in encoded audio time base
	uint64_t scaledPTS = lrint(ptsDiff * (outputRate / (double)audioBuffer->GetClockRate())) + playPTSOffset.value();
	return scaledPTS;
}