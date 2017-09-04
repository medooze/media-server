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


RTPStreamTransponder::RTPStreamTransponder(RTPOutgoingSourceGroup* outgoing,RTPSender* sender)
{
	//No thread
	setZeroThread(&thread);
	running = false;
	
	//No packets
	first = 0;
	base = 0;
	last = 0;
	
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
	
	//OK
	return true;
}

RTPStreamTransponder::~RTPStreamTransponder()
{
	//Stop listeneing
	Close();
	//Clean all pending packets in queue
	while(packets.Length())
		//Deque and delete
		delete (packets.Pop());
}

void RTPStreamTransponder::Close()
{
	ScopedLock lock(mutex);
	
	//Stop
	Stop();

	//Stop listeneing
	if (outgoing) outgoing->RemoveListener(this);
	if (incoming) incoming->RemoveListener(this);

	//Remove sources
	outgoing = NULL;
	incoming = NULL;
	receiver = NULL;
	sender = NULL;
}



void RTPStreamTransponder::onRTP(RTPIncomingSourceGroup* group,RTPPacket* packet)
{
	ScopedLock lock(mutex);

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
	
	//Set normalized seq num
	extSeqNum = base + (extSeqNum - first);
	
	//Clone packet
	RTPPacket* cloned = packet->Clone();
	
	//Set new seq numbers
	cloned->SetSeqNum(extSeqNum);
	cloned->SetSeqCycles(extSeqNum >> 16);

	//Check if it is an VP9 packet
	if (cloned->GetCodec()==VideoCodec::VP9)
	{
		bool mark;
		//Select layer
		if (!selector.Select(packet,extSeqNum,mark))
		       //Drop
		       return;
	       //Reset seq num again
	       cloned->SetSeqNum(extSeqNum);
	       cloned->SetSeqCycles(extSeqNum >> 16);
	       //Set mark
	       cloned->SetMark(mark);
	}
	
	//Change ssrc
	cloned->SetSSRC(outgoing->media.ssrc);
	
	//Add to queue for asyn processing
	packets.Add(cloned);
}

void RTPStreamTransponder::onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc)
{
	ScopedLock lock(mutex);
	//Request update on the incoming
	if (receiver && incoming) receiver->SendPLI(incoming->media.ssrc);
}

void RTPStreamTransponder::SelectLayer(int spatialLayerId,int temporalLayerId)
{
	ScopedLock lock(mutex);

	if (selector.GetSpatialLayer()<spatialLayerId)
		//Request update on the incoming
		if (receiver && incoming) receiver->SendPLI(incoming->media.ssrc);
	selector.SelectSpatialLayer(spatialLayerId);
	selector.SelectTemporalLayer(temporalLayerId);
}


void RTPStreamTransponder::Start()
{
	//We are running
	running = true;

	//Create thread
	createPriorityThread(&thread,run,this,0);
}

void RTPStreamTransponder::Stop()
{
	//Check thred
	if (!isZeroThread(thread))
	{
		//Not running
		running = false;
		
		//Cancel packets wait queue
		packets.Cancel();

		//Signal the pthread this will cause the poll call to exit
		pthread_kill(thread,SIGIO);
		//Wait thread to close
		pthread_join(thread,NULL);
		//Nulifi thread
		setZeroThread(&thread);
	}
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
	//LOck packets
	packets.WaitUnusedAndLock();
	
	//Delete each packet
	while(packets.Length())
		//Delete packet
		delete( packets.Pop());
	
	//Reset first paquet seq num
	first = 0;
	//Store the last send one
	base = last;
	
	//Unlock
	packets.Unlock();
	
}

int RTPStreamTransponder::Run()
{
	Log(">RTPBundleTransport::Run() | [%p]\n",this);

	//Run until canceled
	while(packets.Wait(0))
	{
		//Get packet
		RTPPacket* packet = packets.Pop();
		
		//Send it on transport
		sender->Send(*packet);

		//Get last send seq num
		last = packet->GetExtSeqNum();
		
		//Free mem
		delete(packet);
	}

	Log("<RTPBundleTransport::Run()\n");
	
	return 0;
}


