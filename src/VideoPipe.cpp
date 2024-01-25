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

int VideoPipe::Init(float scaleResolutionDownBy, uint32_t scaleResolutionToHeight, AllowedDownScaling allowedDownScaling)
{
	Log("-VideoPipe::Init() [scaleResolutionDownBy:%f,scaleResolutionDownBy:%d,allowedDownScaling:%d]\n",scaleResolutionDownBy, scaleResolutionToHeight, allowedDownScaling);

	//Lock
	pthread_mutex_lock(&newPicMutex);

	//We are inited
	inited = true;
	
	//Store dinamyc scaling settings
	this->scaleResolutionToHeight = scaleResolutionToHeight;
	this->scaleResolutionDownBy = scaleResolutionDownBy;
	this->allowedDownScaling = allowedDownScaling;

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
	buffer.Clear();
	//imgPos = false;
	//imgNew = false;

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

	buffer.Clear();
	//Free buffers
	//imgBuffer[0].reset();
	//imgBuffer[1].reset();

	//Not capturing anymore
	capturing = false;

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	return true;
}

VideoBuffer::const_shared VideoPipe::GrabFrame(uint32_t timeout)
{
	VideoBuffer::const_shared videoBuffer;

	auto now = getTime();
	auto diff = now - lastPull;
	lastPull = now;

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

	// timespec calced once outside the loop so we wait for the correct timeout duration
	// when handling spurious qakeups
	timespec   ts;
	if (timeout)
	{
		//Calculate timeout
		calcTimout(&ts,timeout);
	}

	//Check if there is a new image, or we need to exit for some other reason
	//
	// Note: This is in a loop as it is required when using pthread_cond_wait calls. You need to check for spurious wakeups
	// See: https://pubs.opengroup.org/onlinepubs/009604599/functions/pthread_cond_timedwait.html
	// 
	// This means we need to check all conditions that may signal the condition to wakeup including:
	// * End() : Setting inited == false
	// * CancelGrabFrame() : Setting cancelledGrab == true
	// * NextFrame() : Adding a new frame setting imgNew = true
	bool timedOut = false;
	while(inited && buffer.Size()==0 && !cancelledGrab && !timedOut)
	{
		int ret = -1;

		//If timeout has been specified
		if (timeout)
		{
			//wait
			ret = pthread_cond_timedwait(&newPicCond,&newPicMutex,&ts);
			if (ret == ETIMEDOUT)
			{
				//Debug("-VideoPipe::GrabFrame() timed out. Processing as normal though\n");
				timedOut = true;
				ret = 0;
			}
		}
		else
		{
			//Wait ad infinitum
			ret = pthread_cond_wait(&newPicCond,&newPicMutex);
		}
		
		if (ret)
		{
			Error("-VideoPipe cond wait error [%d,%d]\n",ret,errno);
		}
	}

	//Reset new image flag
	//imgNew=0;

	// If this was true, handle img as normal (may exist or may not) and clear this flag so we dont cancel the next one as well
	cancelledGrab=0;

	//Get current image
	//videoBuffer = std::move(imgBuffer[imgPos]);
	buffer.PopFront(&videoBuffer);
	size_t bufferSize = buffer.Size();

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	now = getTime();
	auto ldiff = now - lastPull;

	//Check we have a new frame
	if (!videoBuffer)
	{
		//Debug("-VideoPipe::GrabFrame() TDIFF:%llu(%llu) Returning with empty frame\n", diff, ldiff);
		//No frame
		return videoBuffer;
	}
	//Debug("-VideoPipe::GrabFrame() TDIFF:%llu(%llu) returning with frame ts: %llu and buffer size: %u\n", diff, ldiff, videoBuffer->GetTimestamp(), bufferSize);

	//Get scaling factor
	float scale = scaleResolutionDownBy;

	//If we are downscaling to an specific height
	if (scaleResolutionToHeight)
	{
		//Get video height
		auto videoHeight = videoBuffer->GetHeight();

		//Check if we are allowed to downscale given the input height
		if ((allowedDownScaling == AllowedDownScaling::SameOrLower && videoHeight<scaleResolutionToHeight)
			|| (allowedDownScaling == AllowedDownScaling::LowerOnly && videoHeight <= scaleResolutionToHeight))
			//Skip frame
			return nullptr;

		//Change scale to match target height
		scale = (float)videoHeight / scaleResolutionToHeight;
	}

	//If we have a dinamic resize
	if (scale > 0 || videoBuffer->HasNonSquarePixelAspectRatio())
	{
		//Get pixel aspect ratio
		const auto [parNum, parDen] = videoBuffer->GetPixelAspectRatio();

		//Choose which dimension we want to use for adjusting the pixel aspect ratio
		if (parNum>parDen)
		{
			//Check adjusted video size
			videoWidth = ((uint32_t)((videoBuffer->GetWidth() * parNum) / (scale * parDen))) & ~1;
			videoHeight = ((uint32_t)(videoBuffer->GetHeight() / scale)) & ~1;
		} else {
			//Check adjusted video size
			videoWidth = ((uint32_t)(videoBuffer->GetWidth() / scale)) & ~1;
			videoHeight = ((uint32_t)((videoBuffer->GetHeight() * parDen) / (scale * parNum))) & ~1;
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

		//Set timing info from original video buffer
		if (videoBuffer->HasClockRate())
			resized->SetClockRate(videoBuffer->GetClockRate());
		if (videoBuffer->HasTimestamp())
			resized->SetTimestamp(videoBuffer->GetTimestamp());
		if (videoBuffer->HasTime())
			resized->SetTime(videoBuffer->HasTime());

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

	// We need to also set this to identify we want to exit the cond var loop
	// on any current or the next call to GrabFrame
	// 
	// Otherwise without this bool the cond var loop will wakeup, see !imgNew and go back to sleep. 
	cancelledGrab = true;

	//Signal to cancel any pending GrabFrame()
	pthread_cond_signal(&newPicCond);

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

}

int VideoPipe::NextFrame(const VideoBuffer::const_shared& videoBuffer)
{
	auto now = getTime();
	auto diff = now - lastPush;
	lastPush = now;

	//Lock
	pthread_mutex_lock(&newPicMutex);
	now = getTime();
	auto ldiff = now - lastPush;

	if (!buffer.PushBack(videoBuffer))
	{
		Debug("-VideoPipe::NextFrame() TDIFF:%llu(%llu) NextFrame and GrabFrame requests have significant jitter butween them resulting in dropped frames. Adding new videoBuffer with timestamp:%llu resulting in skipping a previously buffered frame that has not yet been consumed\n", diff, ldiff, videoBuffer->GetTimestamp());
	}

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


