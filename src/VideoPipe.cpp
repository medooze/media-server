#include "VideoPipe.h"
#include "log.h"
#include "tools.h"
#include <stdlib.h>
#include <string.h>

VideoPipe::VideoPipe() :
	videoBufferPool(2,4)
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

int VideoPipe::StartVideoCapture(uint32_t width, uint32_t height, uint32_t fps)
{
	Log("-VideoPipe::StartVideoCapture() [%u,%u,%u]\n",width,height,fps);

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Almacenamos el tama�o
	videoWidth = width;
	videoHeight = height;
	videoFPS = fps;

	//Reset pool
	videoBufferPool.SetSize(videoWidth, videoHeight);
	
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
	imgBuffer[0].reset();
	imgBuffer[1].reset();

	//Y no estamos capturando
	capturing = false;

	//Desprotegemos
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

VideoBuffer::const_shared VideoPipe::GrabFrame(uint32_t timeout)
{
	VideoBuffer::const_shared videoBuffer;
	
	//Lock until we have a new picture
	pthread_mutex_lock(&newPicMutex);

	//Si no estamos iniciados
	if (!inited)
	{
		//Logeamos
		Error("-VideoPipe::GrabFrame() | VideoPipe no inited, grab failed\n");
		//Desbloqueamos
		pthread_mutex_unlock(&newPicMutex);
		//Salimos
		return videoBuffer;
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
	videoBuffer = imgBuffer[imgPos];

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	//If we got a frame and it is from a different size
	if (videoBuffer  && (videoBuffer->GetWidth() != videoWidth || videoBuffer->GetHeight() !=videoHeight))
	{
		//Get new buffer
		VideoBuffer::shared resized = videoBufferPool.allocate();

		//Rescale
		scaler.Resize(videoBuffer, resized, true);

		//Swap buffers
		videoBuffer = std::move(resized);
	}

	//Done
	return videoBuffer;
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

int VideoPipe::NextFrame(const VideoBuffer::const_shared& videoBuffer)
{

	//Protegemos
	pthread_mutex_lock(&newPicMutex);

	//Actualizamos el grabPic
	imgBuffer[imgPos] = videoBuffer;

	//Pasamos al siguiente
	imgPos = !imgPos;

	//Hay imagen
	imgNew = true;

	//If we have a dinamic resize
	if (scaleResolutionDownBy > 0)
	{
		//Check adjusted video size
		videoWidth = ((uint32_t)(videoBuffer->GetWidth() / scaleResolutionDownBy)) & ~1;
		videoHeight = ((uint32_t)(videoBuffer->GetHeight() / scaleResolutionDownBy)) & ~1;

		//Debug("-VideoPipe::NextFrame() | Scaling down from [%u,%u] to [%u,%u]\n", videoBuffer->GetWidth(), videoBuffer->GetHeight(), videoWidth, videoHeight);

		//Reset pool
		videoBufferPool.SetSize(videoWidth, videoHeight);
	}

	//Se�alamos
	pthread_cond_signal(&newPicCond);

	//Y desbloqueamos
	pthread_mutex_unlock(&newPicMutex);

	return 1;
}

void VideoPipe::ClearFrame()
{
	//Get new buffer
	VideoBuffer::shared black = videoBufferPool.allocate();

	//Paint in black
	black->Fill(0, (uint8_t)-128, (uint8_t)-128);

	//Put as next
	NextFrame(black);
}
