#include "VideoPipe.h"
#include "log.h"
#include "tools.h"
#include <stdlib.h>
#include <string.h>

VideoPipe::VideoPipe() : 
	// Want a non growing queue
	queue(MaxOutstandingFramesDefault, false),
	videoBufferPool(MaxOutstandingFramesDefault, MaxOutstandingFramesDefault + 2)
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
	Log("-VideoPipe::Init() [scaleResolutionDownBy:%f,scaleResolutionToHeight:%d,allowedDownScaling:%d]\n",scaleResolutionDownBy, scaleResolutionToHeight, allowedDownScaling);

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

void VideoPipe::SetMaxDelay(uint32_t maxDelayMs)
{
	pthread_mutex_lock(&newPicMutex);

	// Note: We dont *know* the ingest fps, so will just assume max is 60fps for now
	// If for whatever reason the egress fps is larger we will know about it here
	int maxFPS = std::max(60, videoFPS);

	// Figure out the max frames we might every have in this queue. 
	// Note: Actual latency values are enforced by the maxDelayMs
	// we are providing an upper limit on the outstanding items in the queue
	// and will resize this queue to be larger in case we increase the latency
	// past what is likely to be supported by the queues default
	size_t maxDelayInFrames = std::ceil((maxDelayMs * maxFPS) / 1000.0);
	Log("-VideoPipe::SetMaxDelay() Setting max delay: %ums (0 indicates no max)\n", maxDelayMs);
	if (queue.size() < maxDelayInFrames)
	{
		Log("-VideoPipe::SetMaxDelay() Increasing size of video pipe queue to: %u from %u to permit new max delay: %ums\n",maxDelayInFrames, queue.size(), maxDelayMs);
		queue.grow(maxDelayInFrames);
	}
	this->maxDelayMs = maxDelayMs;
	pthread_mutex_unlock(&newPicMutex);
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

	do 
	{
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
		while(inited && capturing && queue.length()==0 && !cancelledGrab)
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

		// Need to reset the videoBuffer or when downrating
		// and empty queue will loop in here until another 
		// frame arrives instead of returning no frame yet
		// Also done before the cancelledGrab so it doesnt 
		// return an invalid frame
		videoBuffer.reset();

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

	} while (
		// Make sure condition below is valid to check
		videoFPS != 0
		&& lastGrabbedTimestamp!= NoTimestamp 
		&& videoBuffer 
		&& videoBuffer->HasTimestamp()

		// If ts goes backwards then just return as something is weird
		// Happens when encoder resets without resetting the stream
		// Jumping forwards a big jump will already work fine in the check below
		&& videoBuffer->GetTimestamp() >= lastGrabbedTimestamp

		// Loops dropping frames before the next timestamp to match the capture fps
		//
		// The -1 here is to handle potential quantization of timestamps (we have 
		// seen occasionally with msec accurate clocks) resulting in values differing
		// by 1 occasionally not causing unnecessary drops.
		&& lastGrabbedTimestamp + videoBuffer->GetClockRate()/videoFPS - 1 > videoBuffer->GetTimestamp()
	);

	//Check we have a new frame
	if (!videoBuffer)
	{
		//Unlock
		pthread_mutex_unlock(&newPicMutex);
		//Return no frame
		return videoBuffer;
	}
	
	if (scaleResolutionToHeight || scaleResolutionDownBy) 
	{
		float scale = 0.0f;
		auto srcVideoHeight = videoBuffer->GetHeight();
		const auto [parNum, parDen] = videoBuffer->GetPixelAspectRatio();
		if(scaleResolutionToHeight) 
		{

			if ((allowedDownScaling == AllowedDownScaling::SameOrLower && srcVideoHeight < scaleResolutionToHeight)
				|| (allowedDownScaling == AllowedDownScaling::LowerOnly && srcVideoHeight <= scaleResolutionToHeight))
			{
				pthread_mutex_unlock(&newPicMutex);
				//Skip frame
				return nullptr;
			}
			//Change scale to match target height
			scale = (float)srcVideoHeight / scaleResolutionToHeight;
		}
		else 
		{
			//Check if we are allowed to downscale given the input height
			if ((allowedDownScaling == AllowedDownScaling::SameOrLower && scaleResolutionDownBy < 1.0f)
				|| (allowedDownScaling == AllowedDownScaling::LowerOnly && scaleResolutionDownBy <= 1.0f))
			{
				pthread_mutex_unlock(&newPicMutex);
				//Skip frame
				return nullptr;
			}
			scale = scaleResolutionDownBy;
		}
		//rescaled video height
		videoHeight = (uint32_t)(videoBuffer->GetHeight() / scale) & ~1;
		//rescale width to retain DAR, plus 1 to round up to nearest even number
		videoWidth = (uint32_t)((videoBuffer->GetWidth() * parNum) / (scale * parDen) + 1) & ~1;
		//Debug("-VideoPipe::GrabFrame() | Scaling down from [%u,%u] to [%u,%u] with par [%u,%u]\n", videoBuffer->GetWidth(), videoBuffer->GetHeight(), videoWidth, videoHeight, parNum, parDen);

		//Reset pool
		videoBufferPool.SetSize(videoWidth, videoHeight);
	}

	//If we got a frame and it is from a different size
	if (videoBuffer->GetWidth() != videoWidth || videoBuffer->GetHeight() != videoHeight)
	{
		//Get new buffer
		VideoBuffer::shared resized = videoBufferPool.Acquire();

		//Rescale
		scaler.Resize(videoBuffer, resized, true);

		resized->CopyTimingInfo(videoBuffer);

		//Swap buffers
		videoBuffer = std::move(resized);
	}
  
	//Unlock
	pthread_mutex_unlock(&newPicMutex);
  
	//If we got timestamps in the video buffer
	if (videoBuffer->HasTimestamp())
	{
		// Sometimes video encoders might be reset and NOT the stream causing the timestamps to
		// jump backwards. Lets log this case as it is weird and shouldnt happen often. 
		// If it does happen often and spams we want to know about it
		if (lastGrabbedTimestamp != NoTimestamp && videoBuffer->GetTimestamp() < lastGrabbedTimestamp)
		{
			Warning("-VideoPipe Warning video stream timestamp seemed to go backwards from: %lld to: %lld\n", lastGrabbedTimestamp, videoBuffer->GetTimestamp());
		}
		//Update timestamp
		lastGrabbedTimestamp = videoBuffer->GetTimestamp();
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


		//Do not enqueue more than the max delay packets
	if (maxDelayMs && videoBuffer->HasTimestamp())
	{
		auto maxDelayInClockTicks = (maxDelayMs * videoBuffer->GetClockRate()) / 1000;

		while (!queue.empty() 
			// Dont drop if no valid timestamp or changing clock rates as we cant legit compare them
			&& queue.front()->HasTimestamp()
			&& videoBuffer->GetClockRate() == queue.front()->GetClockRate()

			// Make sure not to get timestamp oveflow in the subtraction
			&& videoBuffer->GetTimestamp() > queue.front()->GetTimestamp()

			// Compare the duration diff to the max delay
			&& (videoBuffer->GetTimestamp() - queue.front()->GetTimestamp()) >= maxDelayInClockTicks
		)
		{
			//Drop first picture
			queue.pop_front();
			++droppedFramesSinceReport;
		}
	}


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
	VideoBuffer::shared black = videoBufferPool.Acquire();

	//Paint in black
	black->Fill(0, (uint8_t)-128, (uint8_t)-128);

	//Put as next
	NextFrame(black);
}
