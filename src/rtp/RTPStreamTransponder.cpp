/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPStreamTransponder.cpp
 * Author: Sergio
 * 
 * Created on 20 de abril de 2017, 18:33
 */

#include "rtp/RTPStreamTransponder.h"
#include "waitqueue.h"
#include "vp8/vp8.h"


RTPStreamTransponder::RTPStreamTransponder(RTPOutgoingSourceGroup* outgoing,RTPSender* sender)
{
	//No thread
	setZeroThread(&thread);
	
	//Store outgoing streams
	this->outgoing = outgoing;
	this->sender = sender;
	ssrc = outgoing->media.ssrc;
	
	//Add us as listeners
	outgoing->AddListener(this);

	//Start
	Start();
}


bool RTPStreamTransponder::SetIncoming(RTPIncomingSourceGroup* incoming, RTPReceiver* receiver)
{
	ScopedLock lock(mutex);
	
	//If they are the same
	if (this->incoming==incoming && this->receiver==receiver)
		//DO nothing
		return false;
	
	Debug(">RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p,ssrc:%d]\n",incoming,receiver,ssrc);
	
	//Remove listener from old stream
	if (this->incoming) 
		this->incoming->RemoveListener(this);
	
	//Reset packets before start listening again
	Reset();
	
	//Store stream and receiver
	this->incoming = incoming;
	this->receiver = receiver;
	
	//Double check
	if (this->incoming)
	{
		//Add us as listeners
		incoming->AddListener(this);
		
		//Request update on the incoming
		if (this->receiver) this->receiver->SendPLI(this->incoming->media.ssrc);
	}
	
	Debug("<RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p]\n",incoming,receiver);
	
	//OK
	return true;
}

RTPStreamTransponder::~RTPStreamTransponder()
{
	//Stop listeneing
	Close();
	//Clean all pending packets in queue
	Reset();
}

void RTPStreamTransponder::Close()
{
	ScopedLock lock(mutex);
	
	Debug(">RTPStreamTransponder::Close()\n");
	
	//Stop listeneing
	if (outgoing) outgoing->RemoveListener(this);
	if (incoming) incoming->RemoveListener(this);

	//Stop main loop
	Stop();
	
	//Remove sources
	outgoing = NULL;
	incoming = NULL;
	receiver = NULL;
	sender = NULL;
	
	Debug("<RTPStreamTransponder::Close()\n");
}



void RTPStreamTransponder::onRTP(RTPIncomingSourceGroup* group,const RTPPacket::shared& packet)
{
	//Double check
	if (!packet)
		//Exit
		return;
	
	//If muted
	if (muted)
		//Skip
		return;

	//Check if it is an empty packet
	if (!packet->GetMediaLength())
	{
		Debug("-StreamTransponder::onRTP dropping empty packet\n");
		//Drop it
		dropped++;
		//Exit
		return;
	}
	
	//Block packet list and selector
	ScopedLock lock(wait);
	
	//Get new seq number
	DWORD extSeqNum = packet->GetExtSeqNum();
	
	//Check if it the first received packet
	if (!firstExtSeqNum)
	{
		//Store seq number
		firstExtSeqNum = extSeqNum;
		//Reset drop counter
		dropped = 0;
		//Get first timestamp
		firstTimestamp = packet->GetTimestamp();
		//If we have a time offest from last sent packet
		if (lastTime)
		{
			//Calculate time diff
			QWORD offset = getTimeDiff(lastTime)/1000;
			//convert it to rtp time and add to base timestamp
			baseTimestamp += offset*packet->GetClockRate()/1000;
		} else {
			//Just clone it
			baseTimestamp = firstTimestamp;
		}
		
		//UltraDebug("-first seq:%lu ts:%llu base:%lu offset:%llu\n",firstExtSeqNum,firstTimestamp,baseExtSeqNum,baseTimestamp);
	}
	
	//Ensure it is not before first one
	if (extSeqNum<firstExtSeqNum)
		//Exit
		return;
	
	//Check if we have a selector and it is not from the same codec
	if (selector && (BYTE)selector->GetCodec()!=packet->GetCodec())
	{
		//Delete it and reset
		delete(selector);
		//Create new selector for codec
		selector = VideoLayerSelector::Create((VideoCodec::Type)packet->GetCodec());
		//Set prev layers
		selector->SelectSpatialLayer(spatialLayerId);
		selector->SelectTemporalLayer(temporalLayerId);
	//Check if we don't have a selector yet
	} else if (!selector) {
		//Create new selector for codec
		selector = VideoLayerSelector::Create((VideoCodec::Type)packet->GetCodec());
		//Select prev layers
		selector->SelectSpatialLayer(spatialLayerId);
		selector->SelectTemporalLayer(temporalLayerId);
	}
	
	bool mark = packet->GetMark();
	
	//Select layer
	if (selector && !selector->Select(packet,mark))
	{
		//One more dropperd
		dropped++;
		//Drop
		return;
	}
	
	//Clone packet
	auto cloned = packet->Clone();
	
	//Set normalized seq num
	extSeqNum = baseExtSeqNum + (extSeqNum - firstExtSeqNum) - dropped;
	
	//Set new seq numbers
	cloned->SetSeqNum(extSeqNum);
	cloned->SetSeqCycles(extSeqNum >> 16);
	
	//Set normailized timestamp
	cloned->SetTimestamp(baseTimestamp + (packet->GetTimestamp()-firstTimestamp));
	
	//Set mark again
	cloned->SetMark(mark);
	
	//Change ssrc
	cloned->SetSSRC(ssrc);
	
	//Disable frame markings
	//TODO: we should change this so the header extension is recvonly and stripted when sending it instead
	cloned->DisableFrameMarkings();
	
	//Rewrite pict id
	//TODO: this should go into the layer selector??
	if (rewritePicId && cloned->GetCodec()==VideoCodec::VP8)
	{
		VP8PayloadDescriptor desc;

		//Parse VP8 payload description
		desc.Parse(cloned->GetMediaData(),cloned->GetMediaLength());
		
		//Check if we have a new pictId
		if (desc.pictureIdPresent && desc.pictureId!=lastPicId)
		{
			//Update ids
			lastPicId = desc.pictureId;
			//Increase picture id
			picId++;
		}
		
		//Check if we a new base layer
		if (desc.temporalLevelZeroIndexPresent && desc.temporalLevelZeroIndex!=lastTl0Idx)
		{
			//Update ids
			lastTl0Idx = desc.temporalLevelZeroIndex;
			//Increase tl0 index
			tl0Idx++;
		}
		
		//Rewrite picture id wrapping to 15 or 7 bits
		desc.pictureId = desc.pictureIdLength==2 ? picId%0x7FFF : picId%0x7F;
		//Rewrite tl0 index
		desc.temporalLevelZeroIndex = tl0Idx;
		
		//Write it back
		desc.Serialize(cloned->GetMediaData(),cloned->GetMediaLength());
	}
		
	//Add to queue for asyn processing
	packets.push(cloned);

	//Signal new packet
	wait.Signal();
}

void RTPStreamTransponder::onEnded(RTPIncomingSourceGroup* group)
{
	ScopedLock lock(mutex);
	
	//If they are the not same
	if (this->incoming!=group)
		//DO nothing
		return;
	
	//Reset packets before start listening again
	Reset();
	
	//Store stream and receiver
	this->incoming = nullptr;
	this->receiver = nullptr;
}

void RTPStreamTransponder::RequestPLI()
{
	ScopedLock lock(mutex);
	//Request update on the incoming
	if (receiver && incoming) receiver->SendPLI(incoming->media.ssrc);
}

void RTPStreamTransponder::onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc)
{
	RequestPLI();
}

void RTPStreamTransponder::SelectLayer(int spatialLayerId,int temporalLayerId)
{
	this->spatialLayerId = spatialLayerId;
	this->temporalLayerId = temporalLayerId;
		
	if (!selector)
		return;
	
	if (selector->GetSpatialLayer()<spatialLayerId)
		//Request update on the incoming
		RequestPLI();
	selector->SelectSpatialLayer(spatialLayerId);
	selector->SelectTemporalLayer(temporalLayerId);
}

void RTPStreamTransponder::Start()
{
	//We are running
	running = true;

	//Reset packet wait
	wait.Reset();
	
	//Create thread
	createPriorityThread(&thread,run,this,0);
}

void RTPStreamTransponder::Stop()
{
	Debug(">RTPStreamTransponder::Stop()\n");
	
	//Check thred
	if (!isZeroThread(thread))
	{
		//Not running
		running = false;
		
		//Cancel packets wait queue
		wait.Cancel();
		
		//Wait thread to close
		pthread_join(thread,NULL);
		//Nulifi thread
		setZeroThread(&thread);
	}
	
	Debug("<RTPStreamTransponder::Stop()\n");
}

void * RTPStreamTransponder::run(void *par)
{
	Log("-RTPStreamTransponder::run() | thread [%d,0x%x]\n",getpid(),par);

	//Block signals to avoid exiting on SIGUSR1
	blocksignals();

        //Obtenemos el parametro
        RTPStreamTransponder *transponder = (RTPStreamTransponder *)par;

        //Ejecutamos
        transponder->Run();
	
	//Exit
	return NULL;
}

void  RTPStreamTransponder::Reset()
{
	Debug(">RTPStreamTransponder::Reset()\n");
		
	//Block packet list and selector
	ScopedLock lock(wait);

	//Delete all packets
	while(!packets.empty())
		//Remove from queue
		packets.pop();
	
	//Reset first paquet seq num and timestamp
	firstExtSeqNum = 0;
	firstTimestamp = 0;
	//Store the last send ones
	baseExtSeqNum = lastExtSeqNum+1;
	baseTimestamp = lastTimestamp;
	//None dropped
	dropped = 0;
	//Not selecting
	if (selector)
	{
		//Delete and reset
		delete(selector);
		selector = NULL;
	}
	//No layer
	spatialLayerId = LayerInfo::MaxLayerId;
	temporalLayerId = LayerInfo::MaxLayerId;
	
	Debug("<RTPStreamTransponder::Reset()\n");
}

int RTPStreamTransponder::Run()
{
	Log(">RTPStreamTransponder::Run() | [%p]\n",this);

	//Lock for waiting for packets
	wait.Lock();

	//Run until canceled
	while(running)
	{
		//Check if we have packets
		if (packets.empty())
		{
			//Wait for more
			wait.Wait(0);
			//Try again
			continue;
		}
		
		//Get first packet
		auto packet = packets.front();

		//Remove packet from list
		packets.pop();
		
		//Get last send seq num and timestamp
		lastExtSeqNum = packet->GetExtSeqNum();
		lastTimestamp = packet->GetTimestamp();
		lastTime = getTime();
		
		//UltraDebug("-last seq:%lu ts:%llu\n",lastExtSeqNum,lastTimestamp);
		
		//Unlock
		wait.Unlock();
			
		//Send it on transport
		sender->Send(packet);

		
		//Lock for waiting for packets
		wait.Lock();
	}
	
	//Unlock
	wait.Unlock();
	
	Log("<RTPStreamTransponder::Run()\n");
	
	return 0;
}

void RTPStreamTransponder::Mute(bool muting)
{
	//Check if we are changing state
	if (muting!=muted)
		//Do nothing
		return;
	//If unmutting
	if (!muting)
		//Request update
		RequestPLI();
	//Update state
	muted = muting;
}
