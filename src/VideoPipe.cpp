#include "VideoPipe.h"
#include "log.h"
#include "tools.h"
#include <stdlib.h>
#include <string.h>

static constexpr size_t MaxOutstandingFrames = 2;
VideoPipe::VideoPipe() : 
	// Want a non growing queue
	queue(MaxOutstandingFrames, false),
	videoBufferPool(MaxOutstandingFrames, MaxOutstandingFrames + 2)
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
	queue.clear();

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

	queue.clear();

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

	// timespec calced once outside the loop so we wait for the correct timeout duration
	// when handling spurious wakeups
	timespec wakeupTime;
	if (timeout)
	{
		//Calculate timeout
		calcTimout(&wakeupTime,timeout);
	}

	// Check if there is a new image, or we need to wakeup for some other reason
	//
	// Note: This is in a loop as is required when using pthread_cond_wait calls. You need to check for 
	// spurious wakeups (which are uncommon) and cant just rely on signal() waking up on event
	// See: https://pubs.opengroup.org/onlinepubs/009604599/functions/pthread_cond_timedwait.html
	// 
	// This means we need to check all conditions in the loop predicate that may signal the condition to wakeup including:
	// * End() : Setting inited == false
	// * CancelGrabFrame() : Setting cancelledGrab == true
	// * NextFrame() : Adding a new frame to the queue
	while(inited && queue.length()==0 && !cancelledGrab)
	{
		int ret = -1;

		//If timeout has been specified
		if (timeout)
		{
			//wait
			ret = pthread_cond_timedwait(&newPicCond,&newPicMutex,&wakeupTime);
			if (ret == ETIMEDOUT)
			{
				// On timeout exit the loop
				break;
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

	if (cancelledGrab)
	{
		//Reset flag	
		cancelledGrab = false;

		pthread_mutex_unlock(&newPicMutex);

		//A canceled grab returns no frame
		return videoBuffer;
	}

	//Get current image if there is anything in the queue
	if (!queue.empty())
	{
		videoBuffer = queue.front();
		queue.pop_front();
	}

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	//Check we have a new frame
	if (!videoBuffer)
		//No frame
		return videoBuffer;

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

	// We need to set this to identify we want to exit the cond var loop
	// on any existing or next call to GrabFrame()
	cancelledGrab = true;

	//Signal to cancel any pending GrabFrame()
	pthread_cond_signal(&newPicCond);

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

}

size_t VideoPipe::NextFrame(const VideoBuffer::const_shared& videoBuffer)
{

	//Lock
	pthread_mutex_lock(&newPicMutex);

	auto now = getTime();

	// Push the new decoded picture onto the queue
	if (queue.full())
		++droppedFramesSinceReport;

	queue.push_back(videoBuffer);
	size_t qsize = queue.length();
	++totalFramesSinceReport;
	
	//Signal any pending grap operation
	pthread_cond_signal(&newPicCond);

	//Unlock
	pthread_mutex_unlock(&newPicMutex);

	// Lets report dropped frames roughly every second if they happened
	if (lastDroppedReport + 1000000LLU <= now)
	{
		if (droppedFramesSinceReport > 0)
		{
			Debug("-VideoPipe::NextFrame() Video frame queue overflowed dropping %u of %u frames per second. Is the encoder keeping up or are we downrating?\n", droppedFramesSinceReport, totalFramesSinceReport);
		}

		droppedFramesSinceReport = 0;
		totalFramesSinceReport = 0;
		lastDroppedReport = now;
	}

	// We will return the size of the queue so it can be used in stats reporting from the decode worker
	return qsize;
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
