#include "VideoDecoderWorker.h"
#include "media.h"
#include "VideoCodecFactory.h"

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
	Log(">VideoDecoderWorker::Decode()\n");

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
		
		//If we don't have codec
		if (!videoDecoder || (videoFrame->GetCodec()!=videoDecoder->type))
		{
			//Create new codec from pacekt
			videoDecoder.reset(VideoCodecFactory::CreateDecoder((VideoCodec::Type)videoFrame->GetCodec()));
				
			//Check we found one
			if (!videoDecoder)
				//Skip
				continue;
		}
		
		//Decode packet
		if(!videoDecoder->Decode(videoFrame))
			//Skip
			continue;
			
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

				//Deinterlace decoded frame
				deinterlacer->Process(videoBuffer);

				//Get any porcessed frame
				while (auto deinterlaced = deinterlacer->GetNextFrame())
				{
					//Set frame times
					deinterlaced->SetTimingInfo(videoBuffer);

					//Check if we are muted
					if (!muted)
					{
						//Sync
						ScopedLock scope(mutex);
						//For each output
						for (auto output : outputs)
							//Send it
							output->NextFrame(deinterlaced);
					}
				}
			} else if (!muted) {
				//Sync
				ScopedLock scope(mutex);
				//For each output
				for (auto output : outputs)
					//Send it
					output->NextFrame(videoBuffer);
			}
		}
	}

	Log("<VideoDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void VideoDecoderWorker::onMediaFrame(const MediaFrame& frame)
{
	//Ensure it is video
	if (frame.GetType()!=MediaFrame::Video)
		return;

	//Put it on the queue
	frames.Add(std::shared_ptr<VideoFrame>(static_cast<VideoFrame*>(frame.Clone())));
}

