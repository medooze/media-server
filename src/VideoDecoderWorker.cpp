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
	Log("VideoDecoderThread [%p]\n",pthread_self());
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
	QWORD		frameTime = (QWORD)-1;
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
		if (ts>frameTime)
		{
			Debug("-lost mark packet ts:%llu frameTime:%llu\n",ts,frameTime);
			//Try to decode what is in the buffer
			videoDecoder->DecodePacket(NULL,0,1,1);
			//Get picture
			BYTE *frame = videoDecoder->GetFrame();
			DWORD width = videoDecoder->GetWidth();
			DWORD height = videoDecoder->GetHeight();
			//Check values
			if (frame && width && height)
			{
				//Set frame size
				//Sync
				ScopedLock scope(mutex);
				//For each output
				for (auto output : outputs)
				{
					//Set frame size
					output->SetVideoSize(width,height);
				
					//Check if muted
					if (!muted)
						//Send it
						output->NextFrame(frame);
				}
			}
		}
		
		//Update frame time
		frameTime = ts;
		
		//Decode packet
		if(!videoDecoder->DecodePacket(packet->GetMediaData(),packet->GetMediaLength(),lost,packet->GetMark()))
			//Waiting for refresh
			waitIntra = true;

		//Check if it is the last packet of a frame
		if(packet->GetMark())
		{
			if (videoDecoder->IsKeyFrame())
				Debug("-Got Intra\n");
			
			//No frame time yet for next frame
			frameTime = (QWORD)-1;

			//Get picture
			BYTE *frame = videoDecoder->GetFrame();
			DWORD width = videoDecoder->GetWidth();
			DWORD height = videoDecoder->GetHeight();
			//Check values
			if (frame && width && height)
			{
				//Sync
				ScopedLock scope(mutex);
				//For each output
				for (auto output : outputs)
				{
					//Set frame size
					output->SetVideoSize(width,height);
				
					//Check if muted
					if (!muted)
						//Send it
						output->NextFrame(frame);
				}
			}
			//Check if we got the waiting refresh
			if (waitIntra && videoDecoder->IsKeyFrame())
				//Do not wait anymore
				waitIntra = false;
		}
	}

	Log("<VideoDecoderWorker::Decode()\n");

	//Exit
	return 0;
}

void VideoDecoderWorker::onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet)
{
	//Put it on the queue
	packets.Add(packet->Clone());
}

void VideoDecoderWorker::onEnded(RTPIncomingMediaStream* stream)
{
	//Cancel packets wait
	packets.Cancel();
}


void VideoDecoderWorker::onBye(RTPIncomingMediaStream* stream)
{
	//Cancel packets wait
	packets.Cancel();
}
