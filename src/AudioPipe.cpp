#include "log.h"
#include "AudioPipe.h"
	
AudioPipe::AudioPipe(DWORD rate)
{
	//Set rate
	nativeRate = rate;
	playRate = rate;
	recordRate = rate;
	
	//Init mutex and cond
	pthread_mutex_init(&mutex,0);
	pthread_cond_init(&cond,0);
}
AudioPipe::~AudioPipe()
{
	CancelRecBuffer();
	StopPlaying();
	StopRecording();
	
	//Free mutex and cond
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}
	
int AudioPipe::StartRecording(DWORD rate)
{
	Log("-AudioPipe start recording [rate:%d]\n",rate);
	
	//Set small cache 20ms
	cache = rate/50;

	//Lock
	pthread_mutex_lock(&mutex);

	//If rate has changed
	if (rate != recordRate)
		//Clear playing buffer
		fifoBuffer.clear();

	//Store recording rate
	recordRate = rate;
	//If we already had an open transcoder
	if (transrater.IsOpen())
		//Close it
		transrater.Close();

	//if rates are different
	if (playRate != recordRate)
		//Open it
		transrater.Open(playRate, recordRate, numChannels);
	//We are recording now
	recording = true;

	//Signal
	pthread_cond_signal(&cond);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}

int AudioPipe::StopRecording()
{
	//If we were recording
	if (!recording)
		//Done
		return false;
	
	Log("-AudioPipe stop recording\n");
	
	//Lock
	pthread_mutex_lock(&mutex);
	
	//Estamos grabando
	recording = false;

	//Signal
	pthread_cond_signal(&cond);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return true;
}


void  AudioPipe::CancelRecBuffer()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Cancel
	canceled = true;

	//Signal
	pthread_cond_signal(&cond);

	//Unloco mutex
	pthread_mutex_unlock(&mutex);
}


int AudioPipe::StartPlaying(DWORD rate, DWORD numChannels)
{
	Log("-AudioPipe start playing [rate:%d,channels:%d]\n", rate, numChannels);

	//Lock
	pthread_mutex_lock(&mutex);

	//Store play rate
	playRate = rate;
	//And number of channels
	this->numChannels = numChannels;

	//If we already had an open transcoder
	if (transrater.IsOpen())
		//Close it
		transrater.Close();

	//if rates are different
	if (playRate != recordRate)
		//Open it
		transrater.Open(playRate, recordRate, numChannels);
	
	//We are playing
	playing = true;
	
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//Exit
	return true;
}

int AudioPipe::StopPlaying()
{
	//Check if playing already
	if (!playing)
		//DOne
		return false;
	
	Log("-AudioPipe stop playing\n");

	//Lock
	pthread_mutex_lock(&mutex);
	//Close transrater
	transrater.Close();
	//We are not playing
	playing = false;
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//Exit
	return true;
}

int AudioPipe::PlayBuffer(SWORD* buffer, DWORD size, DWORD frameTime, BYTE vadLevel)
{
	//Debug("-push %d cache %d\n",size,fifoBuffer.length());

	//Lock
	pthread_mutex_lock(&mutex);

	//Don't do anything if nobody is listening
	if (!recording)
	{
		//Unlock
		pthread_mutex_unlock(&mutex);
		//Ok
		return size;
	}

	//Check if we are transtrating
	SWORD resampled[8192];
	if (transrater.IsOpen())
	{
		DWORD resampledSize = 8192 / numChannels;

		//Proccess
		if (!transrater.ProcessBuffer(buffer, size, resampled, &resampledSize))
		{
			//Error
			pthread_mutex_unlock(&mutex);
			return Error("-AudioPipe could not transrate\n");
		}

		//Update parameters
		buffer = resampled;
		size = resampledSize;
	}

	//Calculate total audio length
	DWORD totalSize = size * numChannels;

	//Get left space
	DWORD left = fifoBuffer.size() - fifoBuffer.length();

	//if not enought
	if (totalSize > left)
	{
		Warning("-AudioPipe::PlayBuffer() | not enought space %d %d\n", fifoBuffer.size(), fifoBuffer.length());
		//Free space
		fifoBuffer.remove(totalSize - static_cast<DWORD>(left / numChannels)* numChannels);
	}

	//Add data to fifo
	fifoBuffer.push(buffer, totalSize);

	//Signal rec
	pthread_cond_signal(&cond);

	//Unlock
	pthread_mutex_unlock(&mutex);

	return size;
}


int AudioPipe::RecBuffer(SWORD* buffer, DWORD size)
{
	//Debug("-pop %d cache %d\n",size,fifoBuffer.length());

	DWORD len = 0;
	DWORD totalSize = 0;

	//Lock
	pthread_mutex_lock(&mutex);
	
	//Ensere we are playing
	while (!playing) 
	{
		//If we have been canceled already
		if (canceled)
		{
			//Remove flag
			canceled = false;
			//Exit
			Log("AudioPipe: RecBuffer cancelled.\n");
			//End
			goto end;
		}
		//Wait for change
		pthread_cond_wait(&cond, &mutex);
	}
	
	//Calculate total audio length
	totalSize = size * numChannels;
	
	//Until we have enought samples
	while (!canceled && recording && (fifoBuffer.length() < totalSize + cache))
	{
		//Wait for change
		pthread_cond_wait(&cond, &mutex);

		//If we have been canceled
		if (canceled)
		{
			//Remove flag
			canceled = false;
			//Exit
			Log("AudioPipe: RecBuffer cancelled.\n");
			//End
			goto end;
		}
	}

	//Get samples from queue
	len = fifoBuffer.pop(buffer, totalSize) / numChannels;

end:
	//Unlock
	pthread_mutex_unlock(&mutex);

	//Debug("-poped %d cache %d\n",size,fifoBuffer.length());

	return len;
}


int AudioPipe::ClearBuffer()
{
	//Lock
	pthread_mutex_lock(&mutex);

	//Clear data
	fifoBuffer.clear();

	//Unlock
	pthread_mutex_unlock(&mutex);
	
	return 1;
}
