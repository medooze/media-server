#include "log.h"
#include "pipeaudiooutput.h"

PipeAudioOutput::PipeAudioOutput()
{
	//Creamos el mutex
	pthread_mutex_init(&mutex,NULL);
}

PipeAudioOutput::~PipeAudioOutput()
{
	//Lo destruimos
	pthread_mutex_destroy(&mutex);
}

int PipeAudioOutput::PlayBuffer(WORD *buffer,DWORD size,DWORD frameTime)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Si no cabe
	if(fifoBuffer.length()+size>1024)
		//Limpiamos
		fifoBuffer.clear();

	//Metemos en la fifo
	fifoBuffer.push(buffer,size);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return size;
}
int PipeAudioOutput::StartPlaying()
{
	return true;
}

int PipeAudioOutput::StopPlaying()
{
	return true;
}

int PipeAudioOutput::GetSamples(WORD *buffer,DWORD num)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Obtenemos la longitud
	int len = fifoBuffer.length();

	//Miramos si hay suficientes
	if (len > num)
		len = num;

	//OBtenemos las muestras
	fifoBuffer.pop(buffer,len);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return len;
}
int PipeAudioOutput::Init()
{
	Log("PipeAudioOutput init\n");

	//Protegemos
	pthread_mutex_lock(&mutex);

	//Iniciamos
	inited = true;

	//Protegemos
	pthread_mutex_unlock(&mutex);

	return true;
} 

int PipeAudioOutput::End()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Terminamos
	inited = false;

	//Se�alizamos la condicion
	//pthread_cond_signal(&newPicCond);

	//Protegemos
	pthread_mutex_unlock(&mutex);

	return true;
} 
