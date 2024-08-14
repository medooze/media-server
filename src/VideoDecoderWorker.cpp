#include "VideoDecoderWorker.h"
#include "media.h"
#include "VideoCodecFactory.h"

VideoDecoderWorker::VideoDecoderWorker() :
	bitrateAcu(1000),
	fpsAcu(1000),
	decodingTimeAcu(1000),
	waitingFrameTimeAcu(1000),
	deinterlacingTimeAcu(1000)
{
}

VideoDecoderWorker::~VideoDecoderWorker()
{
	Stop();
}

int VideoDecoderWorker::Start()
{
	Log("-VideoDecoderWorker::Start()\n");

	//Check if need to restart
	if (decoding)
		//Stop first
		Stop();

	//Start decoding
	decoding = 1;

	//launc thread
	createPriorityThread(&thread,startDecoding,this,0);

	return 1;
}
void * VideoDecoderWorker::startDecoding(void *par)
{
	Log("VideoDecoderThread [%lu]\n",pthread_self());
	//Get worker
	VideoDecoderWorker *worker = (VideoDecoderWorker *)par;
	//Block all signals
	blocksignals();
	//Run
	worker->Decode();
	//Exit
	return NULL;;
}

int  VideoDecoderWorker::Stop()
{
	if (!decoding)
		return 0;
	
	Log(">VideoDecoderWorker::Stop()\n");

	//Stop
	decoding=0;

	//Cancel any pending wait
	frames.Cancel();

	//Esperamos
	pthread_join(thread,NULL);

	Log("<VideoDecoderWorker::Stop()\n");

	return 1;
}

void VideoDecoderWorker::AddVideoOutput(VideoOutput* output)
{
	//Ensure we have a valid value
	if (!output)
		//Done
		return;
		
	ScopedLock scope(mutex);
	//Add it
	outputs.insert(output);

}

void VideoDecoderWorker::RemoveVideoOutput(VideoOutput* output)
{
	ScopedLock scope(mutex);
	//Remove from ouput
	outputs.erase(output);
}

int VideoDecoderWorker::Decode()
{
	DWORD num = 0;

	Log(">VideoDecoderWorker::Decode()\n");

	//Waif for frame time init
	uint64_t waitFrameStart = getTimeMS();

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Wait for an incoming frame
		if (!frames.Wait(0))
			//Done
			break;

		//Get frame from queue
		auto videoFrame = frames.Pop();

		//Check
		if (!videoFrame)
			//Check condition again
			continue;

		//Get waif time end
		uint64_t waitFrameEnd = getTimeMS();

		//Update
		waitingFrameTimeAcu.Update(waitFrameEnd, waitFrameEnd - waitFrameStart);

		//If we don't have codec
		if (!videoDecoder || (videoFrame->GetCodec()!=videoDecoder->type))
		{
			//Create new codec from pacekt
			videoDecoder.reset(VideoCodecFactory::CreateDecoder(videoFrame->GetCodec()));
				
			//Check we found one
			if (!videoDecoder)
				//Skip
				continue;
		}
		
		//Get time before decode
		uint64_t decodeStartTime = waitFrameEnd;

		//Decode packet
		if(!videoDecoder->Decode(videoFrame))
			//Skip
			continue;
			
		//Increase deocder frames
		num ++;

		//Get time after decoding
		uint64_t decodeEndTime = getTimeMS();

		//Calculate encoding time
		decodingTimeAcu.Update(decodeEndTime, decodeEndTime - decodeStartTime);

		//Increase frame counter
		fpsAcu.Update(decodeEndTime, 1);

		//Get picture
		while (auto videoBuffer = videoDecoder->GetFrame())
		{
			//Check if we need to apply deinterlacing
			if (videoBuffer->IsInterlaced())
			{
				//If we didn't had a deinterlacer or frame dimensions have changed (TODO)
				if (!deinterlacer)
				{
					//Create new deinterlacer
					deinterlacer.reset(new Deinterlacer());

					//Start it
					if (!deinterlacer->Start(videoBuffer->GetWidth(), videoBuffer->GetHeight()))
					{
						Error("-VideoDecoderWorker::Decode() | Could not start deinterlacer\n");
						deinterlacer.reset();
						continue;
					}
				}

				//Get time before deinterlacing
				uint64_t deinterlaceStartTime = decodeEndTime;

				//Deinterlace decoded frame
				deinterlacer->Process(videoBuffer);

				//Get time after deinterlacing
				uint64_t deinterlaceEndTime = getTimeMS();

				//Calculate deinterlacing time
				deinterlacingTimeAcu.Update(deinterlaceEndTime, deinterlaceEndTime - deinterlaceStartTime);

				//Get any porcessed frame
				while (auto deinterlaced = deinterlacer->GetNextFrame())
				{
					//Set frame times
					deinterlaced->CopyTimingInfo(videoBuffer);

					//Check if we are muted
					if (!muted)
					{
						//Sync
						ScopedLock scope(mutex);
						//Recalculate stats
						stats.timestamp = deinterlaceEndTime;
						stats.totalDecodedFrames = num;
						stats.fps = fpsAcu.GetInstant();
						stats.maxDecodingTime = decodingTimeAcu.GetMaxValueInWindow();
						stats.avgDecodingTime = static_cast<uint16_t>(decodingTimeAcu.GetInstantMedia());
						stats.maxWaitingFrameTime = waitingFrameTimeAcu.GetMaxValueInWindow();
						stats.avgWaitingFrameTime = static_cast<uint16_t>(waitingFrameTimeAcu.GetInstantMedia());
						stats.maxDeinterlacingTime = deinterlacingTimeAcu.GetMaxValueInWindow();
						stats.avgDeinterlacingTime = static_cast<uint16_t>(deinterlacingTimeAcu.GetInstantMedia());
						//For each output
						for (auto output : outputs)
							//Send it
							output->NextFrame(deinterlaced);
					}
				}
			} else if (!muted) {

				//Calculate deinterlacing time
				deinterlacingTimeAcu.Update(decodeEndTime);

				//Sync
				ScopedLock scope(mutex);
				//Recalculate stats
				stats.timestamp = decodeEndTime;
				stats.totalDecodedFrames = num;
				stats.fps = fpsAcu.GetInstant();
				stats.maxDecodingTime = decodingTimeAcu.GetMaxValueInWindow();
				stats.avgDecodingTime = static_cast<uint16_t>(decodingTimeAcu.GetInstantMedia());
				stats.maxWaitingFrameTime = waitingFrameTimeAcu.GetMaxValueInWindow();
				stats.avgWaitingFrameTime = static_cast<uint16_t>(waitingFrameTimeAcu.GetInstantMedia());
				stats.maxDeinterlacingTime = deinterlacingTimeAcu.GetMaxValueInWindow();
				stats.avgDeinterlacingTime = static_cast<uint16_t>(deinterlacingTimeAcu.GetInstantMedia());
				//For each output
				for (auto output : outputs)
					//Send it
					output->NextFrame(videoBuffer);
			}
		}

		//Waif for frame time init
		waitFrameStart = getTimeMS();
	}

	Log("<VideoDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void VideoDecoderWorker::onMediaFrame(const MediaFrame& frame)
{
	//Ensure it is video
	if (frame.GetType()!=MediaFrame::Video)
	{
		Warning("-VideoDecoderWorker::onMediaFrame()  got wrong frame type: %s [this: %p]\n", MediaFrame::TypeToString(frame.GetType()), this);
		return;
	}

	//Put it on the queue
	frames.Add(std::shared_ptr<VideoFrame>(static_cast<VideoFrame*>(frame.Clone())));
}

VideoDecoderWorker::Stats VideoDecoderWorker::GetStats()
{
	ScopedLock scope(mutex);
	return stats;
}