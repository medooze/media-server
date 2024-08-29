#include "log.h"
#include "pipeaudioinput.h"

PipeAudioInput::PipeAudioInput()
{
	//Creamos el mutex
	pthread_mutex_init(&mutex,0);

 	//Y la condicion
	pthread_cond_init(&cond,0);

	//Init
	inited = false;
	recording = false;
	canceled = false;
	nativeRate = 8000;
}

PipeAudioInput::~PipeAudioInput()
{
	//Creamos el mutex
	pthread_mutex_destroy(&mutex);

 	//Y la condicion
	pthread_cond_destroy(&cond);
}

int PipeAudioInput::ClearBuffer()
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Clear data
	fifoBuffer.clear();

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);
}
int PipeAudioInput::RecBuffer(SWORD *buffer,DWORD size)
{
	int len = 0;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Mientras no tengamos suficientes muestras
	while(recording && (fifoBuffer.length()<size))
	{
		//Esperamos la condicion
		pthread_cond_wait(&cond,&mutex);

		//If we have been canceled
		if (canceled)
		{
			//Remove flag
			canceled = false;
			//Exit
			Log("PipeAudioInput: RecBuffer cancelled.\n");
			//End
			goto end;
		}
	}

	//Get samples from queue
	len = fifoBuffer.pop(buffer,size);

end:
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return len;
}

int PipeAudioInput::StartRecording(DWORD rate)
{
	Log("-PipeAudioInput start recording [rate:%d]\n",rate);

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

int PipeAudioInput::StopRecording()
{
	Log("-PipeAudioInput stop recording\n");
	
	//Bloqueamos
	pthread_mutex_lock(&mutex);
	
	//Estamos grabando
	recording = false;

	//Close transrater
	transrater.Close();
	
	//Se�alamos
	pthread_cond_signal(&cond);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return true;
}

int PipeAudioInput::PutSamples(SWORD *buffer,DWORD size)
{
	SWORD resampled[4096];
	DWORD resampledSize = 4096;

	
	//Block
	pthread_mutex_lock(&mutex);
	
	//If we need to transrate
	if (transrater.IsOpen())
	{
		//Transrate
		if (!transrater.ProcessBuffer(buffer, size, resampled, &resampledSize))
		{
			//Desbloqueamos
			pthread_mutex_unlock(&mutex);
			//Error
			return Error("-PipeAudioInput could not transrate\n");
		}
		//Swith input parameters to resample ones
		buffer = resampled;
		size = resampledSize;
	}

	//Si estamos reproduciendo
	if (recording)
	{
		//Si no cabe
		if (fifoBuffer.length()+size>fifoBuffer.size())
			//Limpiamos
			fifoBuffer.clear();

		//Encolamos
		fifoBuffer.push(buffer,size);

		//Se�alamos
		pthread_cond_signal(&cond);
	}

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return true;

}

int PipeAudioInput::Init(DWORD rate)
{
	Log("-PipeAudioInput init [rate:%d]\n",rate);
	
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Iniciamos
	inited = true;

	//Store native sample rate
	nativeRate = rate;

	//Desprotegemos
	pthread_mutex_unlock(&mutex);

	return true;
}

int PipeAudioInput::End()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//No estamos iniciados
	inited = false;

	//Terminamos
	pthread_cond_signal(&cond);

	//Desprotegemos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return true;
}

void  PipeAudioInput::CancelRecBuffer()
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
