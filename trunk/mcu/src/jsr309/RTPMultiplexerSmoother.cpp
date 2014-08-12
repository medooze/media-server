/* 
 * File:   RTPMultiplexerSmoother.cpp
 * Author: Sergio
 * 
 * Created on 7 de noviembre de 2011, 12:18
 */

#include "RTPMultiplexerSmoother.h"
#include "audio.h"
#include "video.h"
#include "text.h"
#include "log.h"
#include "tools.h"

RTPMultiplexerSmoother::RTPMultiplexerSmoother() : RTPMultiplexer()
{
	//NO session
	inited = false;
	
	//Create objects
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);
}

RTPMultiplexerSmoother::~RTPMultiplexerSmoother()
{
	//End
	Stop();

	//Clear memory
	while(queue.Length()>0)
		//Delete first
		delete(queue.Pop());
	
	//Clean object
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}


int RTPMultiplexerSmoother::Start()
{
	Log("-RTPMultiplexerSmoother start\n");
	
	//Check if we are already inited
	if (inited)
		//End first
		Stop();
	
	//We are inited
	inited = true;

	//Run
	createPriorityThread(&thread,run,this,1);

	return 1;
}

int RTPMultiplexerSmoother::SmoothFrame(const MediaFrame* frame,DWORD duration)
{
	//Check
	if (!frame || !frame->HasRtpPacketizationInfo())
		//Error
		return Error("Frame do not has packetization info");

	//Get info
	const MediaFrame::RtpPacketizationInfo& info = frame->GetRtpPacketizationInfo();

	DWORD codec = 0;
	BYTE *frameData = NULL;
	DWORD frameSize = 0;

	//Depending on the type
	switch(frame->GetType())
	{
		case MediaFrame::Audio:
		{
			//get audio frame
			AudioFrame * audio = (AudioFrame*)frame;
			//Get codec
			codec = audio->GetCodec();
			//Get data
			frameData = audio->GetData();
			//Get size
			frameSize = audio->GetLength();
		}
			break;
		case MediaFrame::Video:
		{
			//get Video frame
			VideoFrame * video = (VideoFrame*)frame;
			//Get codec
			codec = video->GetCodec();
			//Get data
			frameData = video->GetData();
			//Get size
			frameSize = video->GetLength();
		}
			break;
		default:
			return Error("No smoother for frame");
	}

	DWORD frameLength = 0;
	//Calculate total length
	for (int i=0;i<info.size();i++)
		//Get total length
		frameLength += info[i]->GetTotalLength();

	//Calculate bitrate for frame
	DWORD current = 0;
	
	//For each one
	for (int i=0;i<info.size();i++)
	{
		//Get packet
		MediaFrame::RtpPacketization* rtp = info[i];

		//Create rtp packet
		RTPPacketSched *packet = new RTPPacketSched(frame->GetType(),codec);

		//Make sure it is enought length
		if (rtp->GetPrefixLen()+rtp->GetSize()>packet->GetMaxMediaLength())
			//Error
			continue;
		
		//Get pointer to media data
		BYTE* out = packet->GetMediaData();
		//Copy prefic
		memcpy(out,rtp->GetPrefixData(),rtp->GetPrefixLen());
		//Copy data
		memcpy(out+rtp->GetPrefixLen(),frameData+rtp->GetPos(),rtp->GetSize());
		//Set length
		DWORD len = rtp->GetPrefixLen()+rtp->GetSize();
		//Set length
		packet->SetMediaLength(len);
		switch(packet->GetMedia())
		{
			case MediaFrame::Video:
				//Set timestamp
				packet->SetTimestamp(frame->GetTimeStamp()*90);
				break;
			case MediaFrame::Audio:
				//Set timestamp
				packet->SetTimestamp(frame->GetTimeStamp()*8);
				break;
			default:
				//Set timestamp
				packet->SetTimestamp(frame->GetTimeStamp());
		}
		//Check
		if (i+1==info.size())
			//last
			packet->SetMark(true);
		else
			//No last
			packet->SetMark(false);
		//Calculate partial lenght
		current += len;
		//Calculate sending time offset from first frame
		packet->SetSendingTime(current*duration/frameLength);
		//Append it
		queue.Add(packet);
	}

	return 1;
}

int RTPMultiplexerSmoother::Cancel()
{
	//Cancel any pending operation
	queue.Cancel();

	//Cancel waiting
	pthread_cond_signal(&cond);

	//exit
	return 1;
}

int RTPMultiplexerSmoother::Stop()
{
	//Check
	if (!inited)
		return 0;

	Log(">RTPMultiplexerSmoother stop\n");
	
	//Not inited
	inited = false;

	//Cancel any pending send
	Cancel();

	//Wait
	pthread_join(thread,NULL);

	Log("<RTPMultiplexerSmoother stopped\n");

	return 1;
}

void* RTPMultiplexerSmoother::run(void *par)
{
        Log("RTPMultiplexerSmootherThread [%p]\n",pthread_self());
        //Get endpoint
	RTPMultiplexerSmoother *smooth = (RTPMultiplexerSmoother *)par;
	//Run 
        smooth->Run();
	//Exit
	return NULL;
}

int RTPMultiplexerSmoother::Run()
{
	timeval prev;
	timespec wait;
	DWORD	sendingTime = 0;
	
	//Calculate first
	getUpdDifTime(&prev);

	Log(">RTPMultiplexerSmoother run\n");
	
	while(inited)
	{
		//Wait for new frame
		if (!queue.Wait(0))
			//Check again
			continue;

		//Get it
		RTPPacketSched *sched = queue.Pop();

		//Check it
		if (!sched)
			//Exit
			continue;

		//Multiplex
		Multiplex(*(RTPPacket*)sched);

		//Update sending time
		sendingTime = sched->GetSendingTime();

		//Lock
		pthread_mutex_lock(&mutex);

		//Calculate timeout
		calcAbsTimeout(&wait,&prev,sendingTime);

		//Wait next or stopped
		pthread_cond_timedwait(&cond,&mutex,&wait);

		//Unlock
		pthread_mutex_unlock(&mutex);

		//If it was last
		if (sched->GetMark())
			//Update time of the previous frame
			getUpdDifTime(&prev);

		//DElete it
		delete(sched);
	}

	Log("<RTPMultiplexerSmoother run\n");

	return 1;
}
