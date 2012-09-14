#include "log.h"
#include "pipevideooutput.h"
#include <string.h>
#include <stdlib.h>

PipeVideoOutput::PipeVideoOutput(pthread_mutex_t* mutex, pthread_cond_t* cond)
{
	//Nos quedamos con los mutex
	videoMixerMutex = mutex;
	videoMixerCond  = cond;

	//E iniciamos el buffer
	buffer = NULL;
	bufferSize = 0;

	//Ponemos el cambio
	isChanged   = false;
	videoWidth  = 0;
	videoHeight = 0;
}

PipeVideoOutput::~PipeVideoOutput()
{
	//Si estaba reservado
	if (buffer!=NULL)
		//Liberamos memoria
		free(buffer);
}

int PipeVideoOutput::NextFrame(BYTE *pic)
{
	//Check pic
	if (!pic)
		return Error("-PipeVideoOuput called with null frame");

	//Check if wer are inited
	if (!inited)
		//Exit
		return Error("-PipeVideoOutput calling NextFrame without been inited\n");
	
	//Bloqueamos
	pthread_mutex_lock(videoMixerMutex);

	//Copiamos
	memcpy(buffer,pic,bufferSize);

	//Ponemos el cambio
	isChanged = true;

	//Se�alizamos
	pthread_cond_signal(videoMixerCond);

	//Y desbloqueamos
	pthread_mutex_unlock(videoMixerMutex);

	return true;
}

int PipeVideoOutput::SetVideoSize(int width,int height)
{

	//Check it it is the same size
	if ((videoWidth!=width) || (videoHeight!=height))
	{
		//Bloqueamos
		pthread_mutex_lock(videoMixerMutex);

		//Si habia buffer lo liberamos
		if (buffer)
			free(buffer);
		//Guardamos el tama�o
		videoWidth = width;
		videoHeight= height;

		//Calculamos la memoria
		bufferSize = (width*height*3)/2;
		//Alocamos
		buffer = (BYTE*)malloc(bufferSize);

		//Y desbloqueamos
		pthread_mutex_unlock(videoMixerMutex);
	}

	return true;
}

BYTE* PipeVideoOutput::GetFrame()
{
	//QUitamos el cambio
	isChanged = false;

	//Y devolvemos el buffer
	return buffer;
}

int PipeVideoOutput::Init()
{
	//Iniciamos
	inited = true;

	return true;
} 

int PipeVideoOutput::End()
{
	//Terminamos
	inited = false;

	return true;
} 
