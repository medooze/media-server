#include "log.h"
#include "pipeaudiooutput.h"


PipeAudioOutput::PipeAudioOutput(bool calcVAD)
{
	//Store vad flag
	this->calcVAD = calcVAD;
	//No vad score acumulated
	acu = 0;
	//Creamos el mutex
	pthread_mutex_init(&mutex,NULL);
}

PipeAudioOutput::~PipeAudioOutput()
{
	//Lo destruimos
	pthread_mutex_destroy(&mutex);
}

int PipeAudioOutput::PlayBuffer(SWORD *buffer,DWORD size,DWORD frameTime)
{
	int v = 0;

	//Check if we need to calculate it
	if (calcVAD)
		//Calculate vad
		v = vad.CalcVad8khz(buffer,size)*size;

	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get left space
	int left = fifoBuffer.size()-fifoBuffer.length();
	//if not enought
	if(size>left)
		//Free space
		fifoBuffer.remove(size-left);

	//Get initial bump
	if (!acu && v)
		//Two seconds minimum
		acu+=16000;
	//Acumule VAD
	acu += v;

	//Check max
	if (acu>48000)
		//Limit so it can timeout faster
		acu = 48000;

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

int PipeAudioOutput::GetSamples(SWORD *buffer,DWORD num)
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

DWORD PipeAudioOutput::GetVAD(DWORD numSamples)
{
	//Protegemos
	pthread_mutex_lock(&mutex);
	//Get vad value
	DWORD r = acu;
	//Check
	if (acu<numSamples)
		//No vad
		acu = 0;
	else
		//Remove cumulative value
		acu -= numSamples;
	//Protegemos
	pthread_mutex_unlock(&mutex);
	
	//Return
	return r;
}