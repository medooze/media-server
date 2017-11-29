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
	running = false;
	
	//No packets
	first = 0;
	base = 0;
	last = 0;
	dropped = 0;
	selector = NULL;
	spatialLayerId = VideoLayerSelector::MaxLayerId;
	temporalLayerId = VideoLayerSelector::MaxLayerId;
	
	//Store outgoing streams
	this->outgoing = outgoing;
	this->sender = sender;
	
	//No incoming yet
	this->incoming = NULL;
	this->receiver = NULL;
	
	//Add us as listeners
	outgoing->AddListener(this);

	//Start
	Start();
}


bool RTPStreamTransponder::SetIncoming(RTPIncomingSourceGroup* incoming, RTPReceiver* receiver)
{
	ScopedLock lock(mutex);
	
	Debug(">RTPStreamTransponder::SetIncoming() | [incoming:%p,receiver:%p]\n",incoming,receiver);
	
	//Remove listener from old stream
	if (this->incoming) 
		this->incoming->RemoveListener(this);
	
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
	
	//Reset packets
	Reset();
	
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



void RTPStreamTransponder::onRTP(RTPIncomingSourceGroup* group,RTPPacket* packet)
{
	//Lock packets
	ScopedLock lock(wait);
	
	//Double check
	if (!group || !packet)
	{
		//Error
		Error("-StreamTransponder::onRTP [group:%p,packet:%p]\n",group,packet);
		//Exit
		return;
	}
	
	//Double check
	if (!outgoing || !sender) 
	{
		//Error
		Error("-StreamTransponder::onRTP [outgoing:%p,sender:%p]\n",outgoing,sender);
		//Exit
		return;
	}

	//Check if it is an empty packet
	if (!packet->GetMediaLength())
	{
		Debug("-StreamTransponder::onRTP dropping empty packet\n");
		//Drop it
		dropped++;
		//Exit
		return;
	}
	
	//Get new seq number
	DWORD extSeqNum = packet->GetExtSeqNum();
	
	//Check if it the first received packet
	if (!first)
		//Store seq number
		first = packet->GetExtSeqNum();
	
	//Ensure it is not before first one
	if (extSeqNum<first)
	{
		//Error
		Error("-StreamTransponder::onRTP got packet before first [first:%lu,seq:%lu]\n",first,extSeqNum);
		//Exit
		return;
	}
	
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
	RTPPacket* cloned = packet->Clone();
	
	//Set normalized seq num
	extSeqNum = base + (extSeqNum - first) - dropped;
	
	//Set new seq numbers
	cloned->SetSeqNum(extSeqNum);
	cloned->SetSeqCycles(extSeqNum >> 16);
	
	//Set mark again
	cloned->SetMark(mark);
	
	//Change ssrc
	cloned->SetSSRC(outgoing->media.ssrc);

	//Add to queue for asyn processing
	packets.push_back(cloned);
	
	//Signal new packet
	wait.Signal();
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
		
	//Block packet list
	ScopedLock lock(wait);

	//Delete all packets
	auto it = packets.begin();
	
	//Until the end
	while(it!=packets.end())
	{
		//Get packet
		RTPPacket* packet = *it;
		//Remove from list and get next item in list
		packets.erase(it++);
		//Delete packet
		delete(packet);
	}
	
	//Reset first paquet seq num
	first = 0;
	//Store the last send one
	base = last;
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
	spatialLayerId = VideoLayerSelector::MaxLayerId;
	temporalLayerId = VideoLayerSelector::MaxLayerId;
	
	Debug("<RTPStreamTransponder::Reset()\n");
}

int RTPStreamTransponder::Run()
{
	Log(">RTPBundleTransport::Run() | [%p]\n",this);

	//Lock for waiting for packets
	wait.Lock();
	
	//Run until canceled
	while(wait.Wait(0))
	{
		//Until we have packets
		while(packets.size())
		{
			//Get first packet
			RTPPacket* packet = packets.front();

			//Remove packet from list
			packets.pop_front();

			//Send it on transport
			sender->Send(*packet);

			//Get last send seq num
			last = packet->GetExtSeqNum();

			//Free mem
			delete(packet);
		}
	}
	
	//Unlock packets
	wait.Unlock();
	
	Log("<RTPBundleTransport::Run()\n");
	
	return 0;
}


