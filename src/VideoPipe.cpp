#include "VideoPipe.h"
#include "log.h"
#include "tools.h"
#include <stdlib.h>
#include <string.h>

VideoPipe::VideoPipe() :
	videoBufferPool(2,4)
{
	//Init mutex
	pthread_mutex_init(&newPicMutex,0);
	pthread_cond_init(&newPicCond,0);
}

VideoPipe::~VideoPipe()
{
	//Destroy mutex
	pthread_mutex_destroy(&newPicMutex);
	pthread_cond_destroy(&newPicCond);
}

int VideoPipe::Init(float scaleResolutionDownBy)
{
	//scaleResolutionDownBy can't be 0
	if (!scaleResolutionDownBy)
		//Log error and exit
		return Error("-VideoPipe::Init() | scaleResolutionDownBy is 0\n");

	Log("-VideoPipe::Init() [scaleResolutionDownBy:%f]\n",scaleResolutionDownBy);

	//Lock
	pthread_mutex_lock(&newPicMutex);

	//We are inited
	inited = true;
	
	//Store scaleResolutionDownBy
	this->scaleResolutionDownBy = scaleResolutionDownBy;

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

int VideoPipe::End()
{
	
	Log("-VideoPipe::End()\n)");
		
	//Lock
	pthread_mutex_lock(&newPicMutex);

	//We are ended
	inited = false;

	//Exit any pending operation
	pthread_cond_signal(&newPicCond);

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

int VideoPipe::StartVideoCapture(uint32_t width, uint32_t height, uint32_t fps)
{
	Log("-VideoPipe::StartVideoCapture() [%u,%u,%u]\n",width,height,fps);

	//Lock
	pthread_mutex_lock(&newPicMutex);

	//Store new settigns
	videoWidth = width;
	videoHeight = height;
	videoFPS = fps;

	//Reset pool
	videoBufferPool.SetSize(videoWidth, videoHeight);
	
	//No new image yet
	imgPos = false;
	imgNew = false;

	//We are capturing now
	capturing = true;

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

int VideoPipe::StopVideoCapture()
{
	Log("-VideoPipe::StopVideoCapture()\n");

	//Lock
	pthread_mutex_lock(&newPicMutex);

	//Free buffers
	imgBuffer[0].reset();
	imgBuffer[1].reset();

	//Not capturing anymore
	capturing = false;

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

VideoBuffer::const_shared VideoPipe::GrabFrame(uint32_t timeout)
{
	VideoBuffer::const_shared videoBuffer;

	//Lock until we have a new picture
	pthread_mutex_lock(&newPicMutex);

	//Check we have been inited
	if (!inited)
	{
		Error("-VideoPipe::GrabFrame() | VideoPipe no inited, grab failed\n");
		//Unlock
		pthread_mutex_unlock(&newPicMutex);
		//Return no frame
		return videoBuffer;
	}

	//Check if there is any new image
	if(imgNew==0)
	{
		//If timeout has been specified
		if (timeout)
		{
			timespec   ts;
			//Calculate timeout
			calcTimout(&ts,timeout);
			//wait
			int ret = pthread_cond_timedwait(&newPicCond,&newPicMutex,&ts);
			if (ret && ret!=ETIMEDOUT)
				Error("-VideoPipe cond timedwait error [%d,%d]\n",ret,errno);
		} else {
			//Wait ad infinitum
			pthread_cond_wait(&newPicCond,&newPicMutex);
		}
	}

	//Reset new image flag
	imgNew=0;

	//Get current image
	videoBuffer = imgBuffer[imgPos];

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	//Check we have a new frame
	if (!videoBuffer)
		//No frame
		return videoBuffer;

	//If we have a dinamic resize
	if (scaleResolutionDownBy > 0 || videoBuffer->HasNonSquarePixelAspectRatio())
	{
		//Get pixel aspect ratio
		const auto [parNum, parDen] = videoBuffer->GetPixelAspectRatio();

		//Choose which dimension we want to use for adjusting the pixel aspect ratio
		if (parNum>parDen)
		{
			//Check adjusted video size
			videoWidth = ((uint32_t)((videoBuffer->GetWidth() * parNum) / (scaleResolutionDownBy * parDen))) & ~1;
			videoHeight = ((uint32_t)(videoBuffer->GetHeight() / scaleResolutionDownBy)) & ~1;
		} else {
			//Check adjusted video size
			videoWidth = ((uint32_t)(videoBuffer->GetWidth() / scaleResolutionDownBy)) & ~1;
			videoHeight = ((uint32_t)((videoBuffer->GetHeight() * parDen) / (scaleResolutionDownBy * parNum))) & ~1;
		}

		//Debug("-VideoPipe::GrabFrame() | Scaling down from [%u,%u] to [%u,%u] with par [%u,%u]\n", videoBuffer->GetWidth(), videoBuffer->GetHeight(), videoWidth, videoHeight, parNum, parDen);

		//Reset pool
		videoBufferPool.SetSize(videoWidth, videoHeight);
	}

	//If we got a frame and it is from a different size
	if (videoBuffer->GetWidth() != videoWidth || videoBuffer->GetHeight() != videoHeight)
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
	//Lock
	pthread_mutex_lock(&newPicMutex);

	//No image
	imgNew = false;

	//Signal to cancel any pending GrabFrame()
	pthread_cond_signal(&newPicCond);

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

}

int VideoPipe::NextFrame(const VideoBuffer::const_shared& videoBuffer)
{

	//Lock
	pthread_mutex_lock(&newPicMutex);

	//Update current image with frame
	imgBuffer[imgPos] = videoBuffer;

	//Use next buffer
	imgPos = !imgPos;

	//There is an image available for grabbing
	imgNew = true;

	//Signal any pending grap operation
	pthread_cond_signal(&newPicCond);

	//Unlock
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
