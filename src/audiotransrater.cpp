#include "log.h"
#include "audiotransrater.h"

AudioTransrater::AudioTransrater()
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

int AudioTransrater::ProcessBuffer(SWORD * in, DWORD sizeIn, SWORD * out, DWORD * sizeOut)
{
	//Check out
	if (!out || !sizeOut)
		//Error
		return Error("-No output buffer/size [%p,%p]\n",out,sizeOut);

	//Resample
	int err = mcu_resampler_process_interleaved_int(resampler, (spx_int16_t*) in, (spx_uint32_t*) & sizeIn, (spx_int16_t*) out, (spx_uint32_t*) sizeOut);

	//Check error
	if (err)
		return Error("-AudioTransrater: resampling error. ErrCode = %d.\n", err);
	
	//OK
	return 1;
}
