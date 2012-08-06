/* 
 * File:   RTPEndpoint.cpp
 * Author: Sergio
 * 
 * Created on 7 de septiembre de 2011, 12:16
 */

#include <fcntl.h>
#include <signal.h>
#include "log.h"
#include "RTPEndpoint.h"
#include "rtpsession.h"

RTPEndpoint::RTPEndpoint(MediaFrame::Type type) : RTPMultiplexer() , RTPSession(this)
{
        //Store type
        this->type = type;
        //Not inited
        inited = false;
	//Not sending or receiving
	sending = false;
	receiving = false;
	//Not reset
	reseted = false;
	//No time
	prevts = 0;
	timestamp = 0;
        //No codec
        codec = -1;
	//Not joined
	joined = NULL;
	//Get freg
	switch(type)
	{
		case MediaFrame::Audio:
			//Set it
			freq = 8;
			break;
		case MediaFrame::Video:
			//Set it
			freq = 90;
			break;
		case MediaFrame::Text:
			//Set it
			freq = 1;
			break;
	}
}

RTPEndpoint::~RTPEndpoint()
{
        //Check
        if (inited)
                //End it
                End();
}

int RTPEndpoint::Init()
{
        //Check
        if (inited)
                //Exit
                return false;
        
        //Start rtp session
        RTPSession::Init();

        //Inited
        inited = true;

	//Reset
	reseted = true;

	//No time
	timestamp = 0;
	
	//Init time
	getUpdDifTime(&prev);
}

int RTPEndpoint::End()
{
        //Chec
        if (!inited)
                //Exit
                return 0;
        
        //Not inited anymore
        inited = false;
	
        //Detach if joined
	Dettach();

        //Stop
        RTPSession::End();

	//If receiving
	if (receiving)
		//Stop it
		StopReceiving();
}

int RTPEndpoint::StartReceiving()
{
	//Check if inited
	if (!inited)
		//Exit
		return Error("Not initied");
	
        //Check
        if (receiving)
                //Exit
                return Error("Alredy receiving");

        //Inited
        receiving = true;

        //Create thread
	createPriorityThread(&thread,run,this,1);

	//Sedn on reset
	ResetStream();

	//Return listening port
	return 1;
}

int RTPEndpoint::StopReceiving()
{
        //Chec
        if (!receiving)
                //Exit
                return Error("Not receiving");

        //Not inited anymore
        receiving = false;

	//Cancel any pending IO
	pthread_kill(thread,SIGIO);

        //Y unimos
	pthread_join(thread,NULL);

	return 1;
}

int RTPEndpoint::StartSending()
{
	//Check if inited
	if (!inited)
		//Exit
		return Error("Not initied");

	//Check if wer are joined
	if (joined)
		//Rquest a FPU
		joined->Update();
        //Send
	sending = true;

	return 1;
}

int RTPEndpoint::StopSending()
{
        //Not Send
	sending = false;

	return 1;
}

void RTPEndpoint::onRTPPacket(RTPPacket &packet)
{
	//Check
	if (!sending)
		//Exit
		return;
	
        //Get type
        MediaFrame::Type packetType = packet.GetMedia();
        //Check types
        if (type!=packetType)
                //Exit
                return;
        //Check type
        if (packet.GetCodec()!=codec)
        {
                //Store it
                codec = packet.GetCodec();
                //Depending on the type
                switch(packetType)
                {
                        case MediaFrame::Audio:
                                //Set it
                                RTPSession::SetSendingAudioCodec((AudioCodec::Type)codec);
                                break;
                        case MediaFrame::Video:
                                //Set it
                                RTPSession::SetSendingVideoCodec((VideoCodec::Type)codec);
                                break;
                        case MediaFrame::Text:
                                //Set it
                                RTPSession::SetSendingTextCodec((TextCodec::Type)codec);
                                break;
                }
	}

	//Get diference from latest frame
	QWORD dif = getUpdDifTime(&prev);

	//If was reseted
	if (reseted)
	{
		//Get new time
		timestamp += dif*freq/1000;
		//Not reseted
		reseted = false;
		
	} else {
		//Get dif from packet timestamp
		timestamp += packet.GetTimestamp()-prevts;
	}

	//Update prev rtp ts
	prevts = packet.GetTimestamp();

        //Send it
        RTPSession::SendPacket(packet,timestamp);
}

void RTPEndpoint::onResetStream()
{
	//Reseted
	reseted = true;

	//Send emptu packet
	RTPSession::SendEmptyPacket();

	//Remove codec
	codec = -1;
}

void RTPEndpoint::onEndStream()
{
	//Not joined anymore
	joined = NULL;
}

int RTPEndpoint::RunAudio()
{
        BYTE lost;
        AudioCodec::Type codec;
        DWORD timestamp;
        RTPPacket audio(MediaFrame::Audio,0,0);

        while(inited)
        {
		DWORD len = audio.GetMaxMediaLength();
                //Get the packet
                if (!RTPSession::GetAudioPacket(audio.GetMediaData(), &len ,&lost, &codec, &timestamp))
			//Next
			continue;
		//Set length
		audio.SetMediaLength(len);
		//Set codec
		audio.SetCodec(codec);
		audio.SetType(codec);
		//Set timestamp
		audio.SetTimestamp(timestamp);
		//Multiplex
		Multiplex(audio);
        }

        return 1;
}

int RTPEndpoint::RunVideo()
{
        BYTE lost;
        BYTE last;
        VideoCodec::Type codec;
        DWORD timestamp = 0;
        RTPPacket video(MediaFrame::Video,0,0);

        while(inited)
        {
		DWORD len = video.GetMaxMediaLength();
                //Get the packet
                if (!RTPSession::GetVideoPacket(video.GetMediaData(),&len,&last,&lost, &codec, &timestamp))
			continue;
		//Set length
		video.SetMediaLength(len);
                //Set codec
                video.SetCodec(codec);
                video.SetType(codec);
                //Set mark
                video.SetMark(last);
                //Set timestamp
                video.SetTimestamp(timestamp);
                //Multiplex
                Multiplex(video);
        }

        return 1;
}

int RTPEndpoint::RunText()
{
        BYTE lost;
        TextCodec::Type codec;
        DWORD timestamp = 0;
        RTPPacket text(MediaFrame::Text,0,0);

        while(inited)
        {
		DWORD len = text.GetMaxMediaLength();
                //Get the packet
                if (!RTPSession::GetTextPacket(text.GetMediaData(),&len,&lost, &codec, &timestamp))
			continue;
		//Set length
		text.SetMediaLength(len);
                //Set codec
                text.SetCodec(codec);
                text.SetType(codec);
                //Set timestamp
                text.SetTimestamp(timestamp);
                //Multiplex
                Multiplex(text);
        }

        return 1;

}

void* RTPEndpoint::run(void *par)
{
        Log("RTPEndpointThread [%d]\n",getpid());
        //Get endpoint
	RTPEndpoint *end = (RTPEndpoint *)par;
        //Block signal in thread
	blocksignals();
	//Catch
	signal(SIGIO,EmptyCatch);
        //Depending on the type
        switch(end->GetType())
        {
                case MediaFrame::Video:
                        //Run Video
                        pthread_exit((void *)end->RunVideo());
                        break;
                case MediaFrame::Audio:
                        //Run Audio
                        pthread_exit((void *)end->RunAudio());
                        break;
                case MediaFrame::Text:
                        //Run Audio
                        pthread_exit((void *)end->RunText());
                        break;
        }
        
}


int RTPEndpoint::Attach(Joinable *join)
{
	//Check if inited
	if (!inited)
		//Error
		return Error("Not inited");

        //Detach if joined
	if (joined)
		//Remove ourself as listeners
		joined->RemoveListener(this);
	//Store new one
	joined = join;
	//If it is not null
	if (joined)
		//Join to the new one
		joined->AddListener(this);

	//OK
	return 1;
}

int RTPEndpoint::Dettach()
{
        //Detach if joined
	if (joined)
		//Remove ourself as listeners
		joined->RemoveListener(this);
	//Not joined anymore
	joined = NULL;
}

void RTPEndpoint::onFPURequested(RTPSession *session)
{
	//Request update of the stream
	Update();
}

int RTPEndpoint::RequestUpdate()
{
	//Check if joined
	if (joined)
		//Remove ourself as listeners
		joined->Update();
}
