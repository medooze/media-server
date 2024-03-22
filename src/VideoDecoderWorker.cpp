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
	bool		waitIntra = false;

	Log(">VideoDecoderWorker::Decode()\n");

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Obtenemos el paquete
		if (!frames.Wait(0))
			//Done
			break;

		//Get frame in queue
		auto encFrame = frames.Pop();

		//Check
		if (!encFrame)
			//Check condition again
			continue;
		
		//Get timestamp
		DWORD frameTimestamp = encFrame->GetTimestamp();
		DWORD frameTime = encFrame->GetTime();
		DWORD frameClockRate = encFrame->GetClockRate();
		DWORD frameSenderTime = encFrame->GetSenderTime();

		//If we don't have codec
		if (!videoDecoder || (encFrame->GetCodec()!=videoDecoder->type))
		{
			//Create new codec from pacekt
			videoDecoder.reset(VideoCodecFactory::CreateDecoder(encFrame->GetCodec()));
				
			//Check we found one
			if (!videoDecoder)
				//Skip
				continue;
		}
		
		//Decode packet
		if(!videoDecoder->Decode(encFrame->GetData(), encFrame->GetLength()))
			//Waiting for refresh
			waitIntra = true;

			//Check if we got the waiting refresh
			if (waitIntra && videoDecoder->IsKeyFrame())
			{
				Debug("-VideoDecoderWorker::Decode() | Got Intra\n");
				//Do not wait anymore
				waitIntra = false;
			}
			
			//Get picture
			const VideoBuffer::shared& frame = videoDecoder->GetFrame();

			//If no frame received yet
			if (!frame)
				//Get next one
				continue;

			//Set frame times
			frame->SetTime(frameTime);
			frame->SetTimestamp(frameTimestamp);
			frame->SetClockRate(frameClockRate);
			frame->SetSenderTime(frameSenderTime);

			//Check if we need to apply deinterlacing
			if (frame->IsInterlaced())
			{
				//If we didn't had a deinterlacer or frame dimensions have changed (TODO)
				if (!deinterlacer)
				{
					//Create new deinterlacer
					deinterlacer.reset(new Deinterlacer());

					//Start it
					if (!deinterlacer->Start(frame->GetWidth(), frame->GetHeight()))
					{
						Error("-VideoDecoderWorker::Decode() | Could not start deinterlacer\n");
						deinterlacer.reset();
						continue;
					}
				}

				//Deinterlace decoded frame
				deinterlacer->Process(frame);

				//Get any porcessed frame
				while (auto deinterlaced = deinterlacer->GetNextFrame())
				{
					//Set frame times
					//TODO: should this be done inside the interlacer?
					deinterlaced->SetTime(frameTime);
					deinterlaced->SetTimestamp(frameTimestamp);
					deinterlaced->SetClockRate(frameClockRate);
					deinterlaced->SetSenderTime(frameSenderTime);

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
					output->NextFrame(frame);
			}
	}

	Log("<VideoDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void VideoDecoderWorker::onMediaFrame(const MediaFrame& frame)
{
	//Check it's a video frame
	if (frame.GetType() != MediaFrame::Type::Video)
	{
		Warning("-VideoDecoderWorker::onMediaFrame(): got %s frame [this: %p]\n", MediaFrame::TypeToString(frame.GetType()), this);
		return;
	}
	//Clone it
	auto cloned = std::shared_ptr<MediaFrame>(frame.Clone());
	//Put it on the queue
	frames.Add(std::static_pointer_cast<VideoFrame>(cloned));
}
