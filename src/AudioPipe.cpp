#include "log.h"
#include "AudioPipe.h"
	
AudioPipe::AudioPipe(DWORD rate)
{
	//Set rate
	nativeRate = rate;
	
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
	
	//Set small cache 40ms
	cache = rate/40;

	//Bloqueamos
	pthread_mutex_lock(&mutex);
	//Store recording rate
	recordRate = rate;
	//Open transrater
	transrater.Open( nativeRate, recordRate );
	//Estamos grabando
	recording = true;
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return true;
}

int AudioPipe::StopRecording()
{
	Log("-AudioPipe stop recording\n");
	
	//Bloqueamos
	pthread_mutex_lock(&mutex);
	
	//Estamos grabando
	recording = false;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return true;
}


void  AudioPipe::CancelRecBuffer()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Cancel
	canceled = true;

	//Se�alamos
	pthread_cond_signal(&cond);

	//Unloco mutex
	pthread_mutex_unlock(&mutex);
}


int AudioPipe::StartPlaying(DWORD rate)
{
	Log("-AudioPipe start playing [rate:%d]\n",rate);

	//Lock
	pthread_mutex_lock(&mutex);

	//Store play rate
	playRate = rate;

	//If we already had an open transcoder
	if (transrater.IsOpen())
		//Close it
		transrater.Close();

	//if rates are different
	if (playRate!=nativeRate)
		//Open it
		transrater.Open(playRate,nativeRate);
	
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//Exit
	return true;
}

int AudioPipe::StopPlaying()
{
	Log("-AudioPipe stop playing\n");

	//Lock
	pthread_mutex_lock(&mutex);
	//Close transrater
	transrater.Close();
	//Unlock
	pthread_mutex_unlock(&mutex);
	
	//Exit
	return true;
}

int AudioPipe::PlayBuffer(SWORD *buffer,DWORD size,DWORD frameTime, BYTE vadLevel)
{
	//Debug("-push %d cache %d\n",size,fifoBuffer.length());
	
	//Don't do anything if nobody is listening
	if (!recording)
		//Ok
		return size;
	
	SWORD resampled[4096];
	DWORD resampledSize = 4096;

	//Check if we are transtrating
	if (transrater.IsOpen())
	{
		//Proccess
		if (!transrater.ProcessBuffer( buffer, size, resampled, &resampledSize))
			//Error
			return Error("-AudioPipe could not transrate\n");

		//Update parameters
		buffer = resampled;
		size = resampledSize;
	}

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get left space
	DWORD left = fifoBuffer.size()-fifoBuffer.length();

	//if not enought
	if(size>left)
	{
		Error("-not enought space %d %d\n",fifoBuffer.size(),fifoBuffer.length());
		//Free space
		fifoBuffer.remove(size-left);
	}

	//Metemos en la fifo
	fifoBuffer.push(buffer,size);
	
	//Signal rec
	pthread_cond_signal(&cond);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return size;
}

int AudioPipe::RecBuffer(SWORD *buffer,DWORD size)
{
	//Debug("-pop %d cache %d\n",size,fifoBuffer.length());
	
	DWORD len = 0;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Mientras no tengamos suficientes muestras
	while(recording && (fifoBuffer.length()<size+cache))
	{
		//Esperamos la condicion
		pthread_cond_wait(&cond,&mutex);

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
	len = fifoBuffer.pop(buffer,size);

end:
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Debug("-poped %d cache %d\n",size,fifoBuffer.length());

	return len;
}

int AudioPipe::ClearBuffer()
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Clear data
	fifoBuffer.clear();

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);
	
	return 1;
}
