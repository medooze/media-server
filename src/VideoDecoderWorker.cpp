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
	packets.Cancel();

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
	QWORD		frameTimestamp = (QWORD)-1;
	QWORD		frameTime = (QWORD)-1;
	DWORD		frameClockRate = (DWORD)-1;
	QWORD		frameSenderTime = 0;
	bool		waitIntra = false;

	Log(">VideoDecoderWorker::Decode()\n");

	//Mientras tengamos que capturar
	while(decoding)
	{
		//Obtenemos el paquete
		if (!packets.Wait(0))
			//Done
			break;

		//Get packet in queue
		auto packet = packets.Pop();

		//Check
		if (!packet)
			//Check condition again
			continue;
		
		//Get extended sequence number and timestamp
		QWORD ts = packet->GetTimestamp();
		QWORD time = packet->GetTime();
		DWORD clockRate = packet->GetClockRate();
		QWORD senderTime = packet->GetSenderTime();

		//If we don't have codec
		if (!videoDecoder || (packet->GetCodec()!=videoDecoder->type))
		{
			//Create new codec from pacekt
			videoDecoder.reset(VideoCodecFactory::CreateDecoder((VideoCodec::Type)packet->GetCodec()));
				
			//Check we found one
			if (!videoDecoder)
				//Skip
				continue;
		}
		
		//Update frame timestamp
		frameTimestamp = ts;
		frameTime = time;
		frameClockRate = clockRate;
		if (senderTime)
			frameSenderTime = senderTime;
		
		//Decode packet
		if(!videoDecoder->Decode(packet->GetData(), packet->GetLength()))
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
			{
				// Reset the timestamps to be ready for the next frame
				frameTimestamp = (QWORD)-1;
				frameTime = (QWORD)-1;
				frameClockRate = (DWORD)-1;
				frameSenderTime = 0;
				//Get next one
				continue;
			}

			//Set frame times
			frame->SetTime(frameTime);
			frame->SetTimestamp(frameTimestamp);
			frame->SetClockRate(frameClockRate);
			frame->SetSenderTime(senderTime);

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

			// Reset the timestamps to be ready for the next frame
			frameTimestamp = (QWORD)-1;
			frameTime = (QWORD)-1;
			frameClockRate = (DWORD)-1;
			frameSenderTime = 0;
	}

	Log("<VideoDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void VideoDecoderWorker::onMediaFrame(const MediaFrame& frame_)
{
	//Clone it
	auto frame = std::shared_ptr<MediaFrame>(frame_.Clone());

	//Check it's a video frame
	if (frame->GetType() != MediaFrame::Type::Video)
	{
		Warning("-VideoDecoderWorker::onMediaFrame(): got %s frame [this: %p]\n", MediaFrame::TypeToString(frame->GetType()), this);
		return;
	}

	//Put it on the queue
	packets.Add(std::static_pointer_cast<VideoFrame>(frame));
}
