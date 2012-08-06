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
}

PipeAudioInput::~PipeAudioInput()
{
	//Creamos el mutex
	pthread_mutex_destroy(&mutex);

 	//Y la condicion
	pthread_cond_destroy(&cond);
}

int PipeAudioInput::RecBuffer(WORD *buffer,DWORD size)
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
			goto end;
		}
	}

	//Obtenemos el numero de muestras
	len = fifoBuffer.pop(buffer,size);

end:
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return len;
}

int PipeAudioInput::StartRecording()
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Estamos grabando
	recording = true;

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return true;
}

int PipeAudioInput::StopRecording()
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Estamos grabando
	recording = false;

	//Se�alamos
	pthread_cond_signal(&cond);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return true;
}

int PipeAudioInput::PutSamples(WORD *buffer,DWORD size)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Si estamos reproduciendo
	if (recording)
	{
		//Si no cabe
		if (fifoBuffer.length()+size>1024)
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

int PipeAudioInput::Init()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Iniciamos
	inited = true;

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
