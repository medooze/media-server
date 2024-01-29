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
	DWORD		lastSeq = RTPPacket::MaxExtSeqNum;
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
		DWORD seq = packet->GetExtSeqNum();
		QWORD ts = packet->GetExtTimestamp();
		QWORD time = packet->GetTime();
		DWORD clockRate = packet->GetClockRate();

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
		
		//Lost packets since last
		DWORD lost = 0;

		//If not first
		if (lastSeq!=RTPPacket::MaxExtSeqNum)
			//Calculate losts
			lost = seq-lastSeq-1;
		
		//Update last sequence number
		lastSeq = seq;
		
		//If lost some packets or still have not got an iframe
		if(lost)
			//Waiting for refresh
			waitIntra = true;

		//Check if we have lost the last packet from the previous frame by comparing both timestamps
		if (ts>frameTimestamp)
		{
			Debug("-VideoDecoderWorker::Decode() | lost mark packet ts:%llu frameTimestamp:%llu\n",ts,frameTimestamp);
			//Try to decode what is in the buffer
			videoDecoder->DecodePacket(NULL,0,1,1);
			//Get picture
			const VideoBuffer::shared& frame = videoDecoder->GetFrame();
			//Check
			if (frame && !muted)
			{
				//Set frame times
				frame->SetTime(frameTime);
				frame->SetTimestamp(frameTimestamp);
				frame->SetClockRate(frameClockRate);
				//Sync
				ScopedLock scope(mutex);
				//For each output
				for (auto output : outputs)
					//Send it
					output->NextFrame(frame);
			}
		}
		
		//Update frame timestamp
		frameTimestamp = ts;
		frameTime = time;
		frameClockRate = clockRate;
		
		//Decode packet
		if(!videoDecoder->DecodePacket(packet->GetMediaData(),packet->GetMediaLength(),lost,packet->GetMark()))
			//Waiting for refresh
			waitIntra = true;

		//Check if it is the last packet of a frame
		if(packet->GetMark())
		{
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
				//Get next one
				continue;
			}

			//Set frame times
			frame->SetTime(frameTime);
			frame->SetTimestamp(frameTimestamp);
			frame->SetClockRate(frameClockRate);

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
		}
	}

	Log("<VideoDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void VideoDecoderWorker::onRTP(const RTPIncomingMediaStream* stream, const RTPPacket::shared& packet)
{
	//Put it on the queue
	packets.Add(packet->Clone());
}

void VideoDecoderWorker::onEnded(const RTPIncomingMediaStream* stream)
{
	//Cancel packets wait
	packets.Cancel();
}


void VideoDecoderWorker::onBye(const RTPIncomingMediaStream* stream)
{
	//Cancel packets wait
	packets.Cancel();
}
