#include "log.h"
#include "pipevideoinput.h"
#include "tools.h"
#include <stdlib.h>
#include <string.h>

PipeVideoInput::PipeVideoInput()
{
	//Inicializamos los mutex
	pthread_mutex_init(&newPicMutex,0);
	pthread_cond_init(&newPicCond,0);

	//No estamos iniciados
	//inited = false;
	capturing = false;
	grabPic = 0;
	imgBuffer[0] = NULL;
	imgBuffer[1] = NULL;
}

PipeVideoInput::~PipeVideoInput()
{
	//Liberamos los mutex
	pthread_mutex_destroy(&newPicMutex);
	pthread_cond_destroy(&newPicCond);
}

int PipeVideoInput::Init()
{
	Log("PipeVideoInput init\n");

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Iniciamos
	inited = true;

	//Protegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
} 

int PipeVideoInput::End()
{
	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Terminamos
	inited = false;

	//Se�alizamos la condicion
	pthread_cond_signal(&newPicCond);

	//Protegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
} 

int PipeVideoInput::StartVideoCapture(int width,int height,int fps)
{
	Log("-StartVideoCapture [%d,%d,%d]\n",width,height,fps);

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Almacenamos el tama�o
	videoWidth = width;
	videoHeight = height;
	videoSize = (videoWidth*videoHeight*3)/2;
	videoFPS = fps;

	//Creamos el buffer
	imgBuffer[0] = (BYTE *)malloc(videoSize);
	imgBuffer[1] = (BYTE *)malloc(videoSize);

	//El inicio
	imgPos = false;
	imgNew = false;
	grabPic = 0;

	//Estamos capturando
	capturing = true;

	//Desprotegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

int PipeVideoInput::StopVideoCapture()
{
	Log("-StopVideoCapture\n");

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//LIberamos los buffers
	if (imgBuffer[0])
		free(imgBuffer[0]);
	if (imgBuffer[1])
		free(imgBuffer[1]);

	//Y no estamos capturando
	capturing = false;

	//Clear flags
	grabPic = NULL;

	//Desprotegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

VideoBuffer PipeVideoInput::GrabFrame(DWORD timeout)
{ 
	VideoBuffer pic;

	//Bloqueamos para ver si hay un nuevo picture
	pthread_mutex_lock(&newPicMutex);

	//Si no estamos iniciados
	if (!inited)
	{
		//Logeamos
		Error("PipeVideoInput no inited, grab failed\n");
		//Desbloqueamos
		pthread_mutex_unlock(&newPicMutex);
		//Salimos
		return pic;
	}
	
	//Miramos a ver si hay un nuevo pict
	if(imgNew==0)
	{
		//If timeout has been specified
		if (timeout)
		{
			timespec   ts;
			//Calculate timeout
			calcTimout(&ts,timeout);
			//wait
			pthread_cond_timedwait(&newPicCond,&newPicMutex,&ts);
		} else {
			//Wait ad infinitum
			pthread_cond_wait(&newPicCond,&newPicMutex);
		}
	}

	//Lo vamos a consumir
	imgNew=0;

	//Nos quedamos con el puntero antes de que lo cambien
	pic.width	= videoWidth;
	pic.height	= videoHeight;
	pic.buffer	= grabPic;

	//Y liberamos el mutex
	pthread_mutex_unlock(&newPicMutex);

	return pic;
}

void  PipeVideoInput::CancelGrabFrame()
{
	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//No image
	imgNew = false;
	grabPic = NULL;

	//Se�alamos
	pthread_cond_signal(&newPicCond);

	//Unloco mutex
	pthread_mutex_unlock(&newPicMutex);
	
}

int PipeVideoInput::SetFrame(BYTE * buffer, int width, int height)
{
	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Si estamos capturamos
	if (capturing)
	{
		//Actualizamos el grabPic
		grabPic = imgBuffer[imgPos];

		//Pasamos al siguiente
		imgPos = !imgPos;

		//Copy & Resize
		resizer.Resize(buffer,width,height,grabPic,videoWidth,videoHeight,true);
		
		//Hay imagen
		imgNew = true;
		//Se�alamos
		pthread_cond_signal(&newPicCond);
	} 

	//Y desbloqueamos
	pthread_mutex_unlock(&newPicMutex);

	return 1;
}
