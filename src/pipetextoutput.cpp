#include "log.h"
#include "pipetextoutput.h"
#include "text.h"

PipeTextOutput::PipeTextOutput()
{
	//Creamos el mutex
	pthread_mutex_init(&mutex,NULL);
}

PipeTextOutput::~PipeTextOutput()
{
	//Lo destruimos
	pthread_mutex_destroy(&mutex);
}

int PipeTextOutput::SendFrame(TextFrame& frame)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Get string
	DWORD len = frame.GetWLength();

	//Si no cabe
	if(fifoBuffer.length()+len>1024)
		//Limpiamos
		fifoBuffer.clear();

	//Metemos en la fifo
	fifoBuffer.push(frame.GetWChar(),len);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return len;
}

int PipeTextOutput::PeekText(wchar_t *buffer,DWORD size)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Obtenemos la longitud
	int len = fifoBuffer.length();

	//Miramos si hay suficientes
	if (len > size)
		//Set maximun
		len = size;

	//OBtenemos las muestras
	fifoBuffer.peek(buffer,len);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return len;
}

int PipeTextOutput::SkipText(DWORD size)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Obtenemos la longitud
	int len = fifoBuffer.length();

	//Miramos si hay suficientes
	if (len > size)
		//Set maximun
		len = size;

	//OBtenemos las muestras
	fifoBuffer.remove(len);

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return len;
}

int PipeTextOutput::ReadText(wchar_t *buffer,DWORD size)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Obtenemos la longitud
	int len = fifoBuffer.length();

	//Miramos si hay suficientes
	if (len > size)
		//Set maximun
		len = size;

	//OBtenemos las muestras
	fifoBuffer.pop(buffer,len);
	
	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return len;
}

int PipeTextOutput::Length()
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Obtenemos la longitud
	int len = fifoBuffer.length();

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return len;
}

int PipeTextOutput::Init()
{
	Log("PipeTextOutput init\n");

	//Protegemos
	pthread_mutex_lock(&mutex);

	//Iniciamos
	inited = true;

	//Protegemos
	pthread_mutex_unlock(&mutex);

	return true;
} 

int PipeTextOutput::End()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Terminamos
	inited = false;

	//Protegemos
	pthread_mutex_unlock(&mutex);

	return true;
} 
