/* 
 * File:   RTPSmoother.cpp
 * Author: Sergio
 * 
 * Created on 7 de noviembre de 2011, 12:18
 */

#include "RTPSmoother.h"
#include "audio.h"
#include "video.h"
#include "text.h"
#include "log.h"
#include "tools.h"

RTPSmoother::RTPSmoother()
{
	//NO session
	session = NULL;
	inited = false;
	
	//Create objects
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);
}

RTPSmoother::~RTPSmoother()
{
	//If still inited
	if (inited)
		//End
		End();
	
	//Clean object
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}


int RTPSmoother::Init(RTPSession *session)
{
	//Check if we are already inited
	if (inited)
		//End first
		End();
	
	//Store it
	this->session = session;

	//We are inited
	inited = true;

	//Run
	createPriorityThread(&thread,run,this,1);

	return 1;
}

int RTPSmoother::SendFrame(MediaFrame* frame,DWORD duration)
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
	DWORD rate = 1;

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
			//Set default rate
			rate = 8000;
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
			//Set default rate
			rate = 90000;
		}
			break;
		default:
			return Error("No smoother for frame");
	}

	DWORD frameLength = 0;
	//Calculate total length
	for (int i=0;i<info.size();i++)
		//Get total length
		frameLength += info[i].GetTotalLength();

	DWORD current = 0;
	
	//For each one
	for (int i=0;i<info.size();i++)
	{
		//Get packet
		const MediaFrame::RtpPacketization& rtp = info[i];

		//Create rtp packet
		RTPPacketSched::shared packet = std::make_shared<RTPPacketSched>(frame->GetType(),codec);

		//Make sure it is enought length
		if (rtp.GetTotalLength()>packet->GetMaxMediaLength())
		{
			Error("RTP payload too big [%d,%d]\n",rtp.GetTotalLength(),packet->GetMaxMediaLength());
			//Error
			continue;
		}
		
		//Set data
		packet->SetPayload(frameData+rtp.GetPos(),rtp.GetSize());
		//Add prefix
		packet->PrefixPayload(rtp.GetPrefixData(),rtp.GetPrefixLen());
		//Set other values
		packet->SetExtTimestamp(frame->GetTimeStamp());
		//Set clock rate
		packet->SetClockRate(frame->GetClockRate());
		//Check
		if (i+1==info.size())
			//last
			packet->SetMark(true);
		else
			//No last
			packet->SetMark(false);
		//Calculate sending time offset from first frame
		packet->SetSendingTime(rtp.GetPos()*duration/frameLength);
		//Calculate partial lenght
		current += rtp.GetPrefixLen()+rtp.GetSize();
		//Append it
		queue.Add(packet);
	}

	return 1;
}

int RTPSmoother::Cancel()
{
	//Cancel any pending operation
	queue.Cancel();

	//Cancel waiting
	pthread_cond_signal(&cond);

	//exit
	return 1;
}

int RTPSmoother::End()
{
	//Check
	if (!inited)
		return 0;
	
	//Not inited
	inited = false;

	//Cancel any pending send
	Cancel();

	//Wait
	pthread_join(thread,NULL);

	return 1;
}

void* RTPSmoother::run(void *par)
{
        Log("RTPSmootherThread [%p]\n",pthread_self());
        //Get endpoint
	RTPSmoother *smooth = (RTPSmoother *)par;
	//Run 
        smooth->Run();
	//Exit
	return NULL;
}

int RTPSmoother::Run()
{
	timeval prev;
	timespec wait;
	DWORD	sendingTime = 0;
	
	//Calculate first
	getUpdDifTime(&prev);

	Log(">RTPSmoother run\n");
	
	while(inited)
	{
		//Wait for new frame
		if (!queue.Wait(0))
			//Check again
			continue;

		//Get it
		auto sched = queue.Pop();

		//Check it
		if (!sched)
			//Exit
			continue;
		
		//Update sending time
		sendingTime = sched->GetSendingTime();
		
		//Send it
		session->SendPacket(sched);

		//If it was not last
		if (!sched->GetMark())
		{
			//If we have to sleep
			if (sendingTime)
			{
				//Lock
				pthread_mutex_lock(&mutex);

				//Calculate timeout
				calcAbsTimeout(&wait,&prev,sendingTime);

				//Wait next or stopped
				pthread_cond_timedwait(&cond,&mutex,&wait);

				//Unlock
				pthread_mutex_unlock(&mutex);
			}
		} else {
			//Update time of the previous frame
			DWORD frameTime = getUpdDifTime(&prev)/1000;
			//Check queue length, it should be empty
			if (queue.Length()>0)
				//Log it
				Debug("-RTPSmoother lagging behind [enqueued:%d,frameTime:%u,sendingTime:%u]\n",queue.Length(),frameTime,sendingTime);
		}
	}

	Log("<RTPSmoother run\n");

	return 1;
}
