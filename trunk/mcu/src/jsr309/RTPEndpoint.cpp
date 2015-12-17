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
#include "codecs.h"

RTPEndpoint::RTPEndpoint(MediaFrame::Type type) : RTPMultiplexer() , RTPSession(type,this)
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
	
	//OK
	return 1;
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
	
	//OK
	return 1;
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

	//Cancel grab
	CancelGetPacket();

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
		//Set it
                RTPSession::SetSendingCodec(codec);
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

int RTPEndpoint::Run()
{
        while(receiving)
        {
                //Get the packet
		RTPPacket* packet = RTPSession::GetPacket();
		//Check packet
		if (!packet)
			//Next
			continue;
		//Check type
		if (packet->GetCodec()==VideoCodec::RED)
		{

			//Get primary data
			RTPPacket *primary = ((RTPRedundantPacket*)packet)->CreatePrimaryPacket();
//			Log("-RED %d %s\n",primary->GetType(),VideoCodec::GetNameFor((VideoCodec::Type)primary->GetCodec()));
			//Multiplex only primary data
			Multiplex(*primary);
			//Delete it
			delete(primary);
		} else {
//			if (packet->GetMedia()==MediaFrame::Video) Log("-PRI %d %s\n",packet->GetType(),VideoCodec::GetNameFor((VideoCodec::Type)packet->GetCodec()));
			//Multiplex
			Multiplex(*packet);
		}
		//Delete ti
		delete(packet);
        }

        return 1;
}

void* RTPEndpoint::run(void *par)
{
        Log("RTPEndpointThread [%p]\n",pthread_self());
        //Get endpoint
	RTPEndpoint *end = (RTPEndpoint *)par;
        //Block signal in thread
	blocksignals();
	//Catch
	signal(SIGIO,EmptyCatch);
	//Run
	end->Run();
	//Exit
	return NULL;
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
	
	//OK
	return 1;
}

void RTPEndpoint::onFPURequested(RTPSession *session)
{
	Log("-onFPURequested [joined:%p]\n",joined);
	//Check if joined
	if (joined)
		//Request update
		joined->Update();
}

void RTPEndpoint::onReceiverEstimatedMaxBitrate(RTPSession *session,DWORD estimation)
{
	//Check if joined
       if (joined)
               //Request update
               joined->SetREMB(estimation);
}

void RTPEndpoint::onTempMaxMediaStreamBitrateRequest(RTPSession *session,DWORD estimation,DWORD overhead)
{
	//Check if joined
       if (joined)
               //Request update
               joined->SetREMB(estimation);
}

void RTPEndpoint::Update()
{
	//Update
	RequestFPU();
}

void RTPEndpoint::SetREMB(DWORD estimation)
{
	//Check if we have an estimator
	if (remoteRateEstimator)
		//Update temporal limit
		remoteRateEstimator->SetTemporalMaxLimit(estimation);

}

int RTPEndpoint::RequestUpdate()
{
	//Request FIR
	return RequestFPU();
}
