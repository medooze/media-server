#include <list>
#include "log.h"
#include "pipetextinput.h"
#include "codecs.h"

PipeTextInput::PipeTextInput()
{
	//Creamos el mutex
	pthread_mutex_init(&mutex,0);

 	//Y la condicion
	pthread_cond_init(&cond,0);

	//No estamos iniciados
	inited = false;
}

PipeTextInput::~PipeTextInput()
{
	//Clean any frame in the que
	while(frames.size())
	{
		//delete delete
		delete(frames.front());
		//Deque
		frames.pop_front();
	}

	//Creamos el mutex
	pthread_mutex_destroy(&mutex);

 	//Y la condicion
	pthread_cond_destroy(&cond);
}

void PipeTextInput::Cancel()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//No estamos iniciados
	inited = false;

	//Terminamos
	pthread_cond_signal(&cond);

	//Desprotegemos
	pthread_mutex_unlock(&mutex);
}

TextFrame* PipeTextInput::GetFrame(DWORD timeout)
{
	TextFrame *frame = NULL;
	timespec ts;
	
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Id we do not have enougth text  samples
	if (!frames.size())
	{
		//Check timeout
		if (timeout)
		{
			//Calculate timeout
			calcTimout(&ts,timeout);
			//Esperamos la condicion
			pthread_cond_timedwait(&cond,&mutex,&ts);
		} else {
			//Esperamos la condicion
			pthread_cond_wait(&cond,&mutex);
		}
	}

	if (frames.size())
	{
		//Get fist
		frame = frames.front();
		//Dequeue
		frames.pop_front();
	}

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	return frame;
}

int PipeTextInput::WriteText(const std::wstring &str)
{
	//WriteText
	return WriteText(str.c_str(),str.length());
}

int PipeTextInput::WriteText(const wchar_t *data,DWORD size)
{
	//Bloqueamos
	pthread_mutex_lock(&mutex);

	//Si estamos reproduciendo
	if (inited)
	{
		//Pop new frame
		frames.push_back(new TextFrame(getDifTime(&first)/1000,data,size));

		//Signal write
		pthread_cond_signal(&cond);
	} else
		Log("Not inited\n");

	//Desbloqueamos
	pthread_mutex_unlock(&mutex);

	//Salimos
	return true;
}

int PipeTextInput::Init()
{
	//Protegemos
	pthread_mutex_lock(&mutex);

	//Iniciamos
	inited = true;

	//Set first timestamp
	getUpdDifTime(&first);

	//Desprotegemos
	pthread_mutex_unlock(&mutex);

	return true;
}

int PipeTextInput::End()
{
	Log(">PipeTextInput End\n");

	//Protegemos
	pthread_mutex_lock(&mutex);

	//No estamos iniciados
	inited = false;

	//Terminamos
	pthread_cond_signal(&cond);

	//Desprotegemos
	pthread_mutex_unlock(&mutex);

	Log("<PipeTextInput Ended\n");

	//Salimos
	return true;
}
