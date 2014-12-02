/* 
 * File:   videopipe.cpp
 * Author: Sergio
 * 
 * Created on 19 de marzo de 2013, 16:08
 */

#include "videopipe.h"
#include "log.h"
#include "tools.h"
#include <stdlib.h>
#include <string.h>

VideoPipe::VideoPipe()
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

VideoPipe::~VideoPipe()
{
	//Liberamos los mutex
	pthread_mutex_destroy(&newPicMutex);
	pthread_cond_destroy(&newPicCond);
}

int VideoPipe::Init()
{
	Log("VideoPipe init\n");

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Iniciamos
	inited = true;

	//Protegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

int VideoPipe::End()
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

int VideoPipe::StartVideoCapture(int width,int height,int fps)
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

int VideoPipe::StopVideoCapture()
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

BYTE* VideoPipe::GrabFrame(DWORD timeout)
{
	BYTE *pic;

	//Bloqueamos para ver si hay un nuevo picture
	pthread_mutex_lock(&newPicMutex);

	//Si no estamos iniciados
	if (!inited)
	{
		//Logeamos
		Error("VideoPipe no inited, grab failed\n");
		//Desbloqueamos
		pthread_mutex_unlock(&newPicMutex);
		//Salimos
		return NULL;
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
	pic=grabPic;

	//Y liberamos el mutex
	pthread_mutex_unlock(&newPicMutex);

	return pic;
}

void  VideoPipe::CancelGrabFrame()
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

DWORD VideoPipe::GetBufferSize()
{
	return (videoWidth*videoHeight*3)/2;
}

int VideoPipe::SetVideoSize(int width, int height)
{
	//Set current values
	inputWidth  = width;
	inputHeight = height;
}

int VideoPipe::NextFrame(BYTE * buffer)
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
		resizer.Resize(buffer,inputWidth,inputHeight,grabPic,videoWidth,videoHeight,true);

		//Hay imagen
		imgNew = true;
		//Se�alamos
		pthread_cond_signal(&newPicCond);
	}

	//Y desbloqueamos
	pthread_mutex_unlock(&newPicMutex);

	return 1;
}

void VideoPipe::ClearFrame()
{
	//Protegemos
	pthread_mutex_lock(&newPicMutex);
	
	//Actualizamos el grabPic
	grabPic = imgBuffer[imgPos];

	//Pasamos al siguiente
	imgPos = !imgPos;

	//Get number of pixels
	DWORD num = videoWidth*videoHeight;

	// paint the background in black for YUV
	memset(grabPic		, 0		, num);
	memset(grabPic+num	, (BYTE) -128	, num/2);

	//Ponemos el cambio
	imgNew = true;

	//Se�alizamos
	pthread_cond_signal(&newPicCond);

	//Y desbloqueamos
	pthread_mutex_unlock(&newPicMutex);
}