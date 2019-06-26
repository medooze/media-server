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
}

VideoPipe::~VideoPipe()
{
	//Liberamos los mutex
	pthread_mutex_destroy(&newPicMutex);
	pthread_cond_destroy(&newPicCond);
}

int VideoPipe::Init(float scaleResolutionDownBy)
{
	Log("-VideoPipe::Init() [scaleResolutionDownBy:%f]\n",scaleResolutionDownBy);

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Iniciamos
	inited = true;
	
	//Store scaleResolutionDownBy
	this->scaleResolutionDownBy = scaleResolutionDownBy;

	//Protegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

int VideoPipe::End()
{
	
	Log("-VideoPipe::End()\n)");
		
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
	Log("-VideoPipe::StartVideoCapture() [%d,%d,%d]\n",width,height,fps);

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Almacenamos el tama�o
	videoWidth = width;
	videoHeight = height;
	videoSize = (videoWidth*videoHeight*3)/2;
	videoFPS = fps;

	//Creamos el buffer
	imgBuffer[0].width  = videoWidth;
	imgBuffer[0].height = videoHeight;
	imgBuffer[0].buffer = (BYTE *)malloc(videoSize);
	imgBuffer[1].width  = videoWidth;
	imgBuffer[1].height = videoHeight;
	imgBuffer[1].buffer = (BYTE *)malloc(videoSize);

	//El inicio
	imgPos = false;
	imgNew = false;

	//Estamos capturando
	capturing = true;

	//Desprotegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

int VideoPipe::StopVideoCapture()
{
	Log("-VideoPipe::StopVideoCapture()\n");

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//LIberamos los buffers
	if (imgBuffer[0].buffer)
		free(imgBuffer[0].buffer);
	if (imgBuffer[1].buffer)
		free(imgBuffer[1].buffer);

	//Y no estamos capturando
	capturing = false;

	//Desprotegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

VideoBuffer VideoPipe::GrabFrame(DWORD timeout)
{
	VideoBuffer pic;
	
	//Bloqueamos para ver si hay un nuevo picture
	pthread_mutex_lock(&newPicMutex);

	//Si no estamos iniciados
	if (!inited)
	{
		//Logeamos
		Error("-VideoPipe::GrabFrame() | VideoPipe no inited, grab failed\n");
		//Desbloqueamos
		pthread_mutex_unlock(&newPicMutex);
		//Salimos
		return std::move(pic);
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
	pic.width	= imgBuffer[imgPos].width;
	pic.height	= imgBuffer[imgPos].height;
	pic.buffer	= imgBuffer[imgPos].buffer;

	//Y liberamos el mutex
	pthread_mutex_unlock(&newPicMutex);

	return std::move(pic);
}

void  VideoPipe::CancelGrabFrame()
{
	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//No image
	imgNew = false;

	//Se�alamos
	pthread_cond_signal(&newPicCond);

	//Unloco mutex
	pthread_mutex_unlock(&newPicMutex);

}

int VideoPipe::SetVideoSize(int width, int height)
{
	//Set current values
	inputWidth  = width;
	inputHeight = height;
	return 1;
}

int VideoPipe::NextFrame(BYTE * buffer)
{
	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Si estamos capturamos
	if (capturing)
	{
		//Actualizamos el grabPic
		auto &grabPic = imgBuffer[imgPos];
		
		//If we have a dinamic resize
		if (scaleResolutionDownBy>0)
		{
			//Check adjusted video size
			DWORD width  = ((DWORD)(inputWidth/scaleResolutionDownBy)) & ~1;
			DWORD height = ((DWORD)(inputHeight/scaleResolutionDownBy)) & ~1;
			//If it is not current
			if (width!=grabPic.width || height!=grabPic.height)
			{
				grabPic.width  = width;
				grabPic.height = height;
				grabPic.buffer = (BYTE *)realloc(grabPic.buffer,grabPic.GetBufferSize());
			}
		}

		//Pasamos al siguiente
		imgPos = !imgPos;

		//Copy & Resize
		resizer.Resize(buffer,inputWidth,inputHeight,grabPic.buffer,grabPic.width,grabPic.height,true);

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
	auto &grabPic = imgBuffer[imgPos];

	//Pasamos al siguiente
	imgPos = !imgPos;

	//Get number of pixels
	DWORD num = grabPic.width*grabPic.height;

	// paint the background in black for YUV
	memset(grabPic.buffer		, 0		, num);
	memset(grabPic.buffer+num	, (BYTE) -128	, num/2);

	//Ponemos el cambio
	imgNew = true;

	//Se�alizamos
	pthread_cond_signal(&newPicCond);

	//Y desbloqueamos
	pthread_mutex_unlock(&newPicMutex);
}
