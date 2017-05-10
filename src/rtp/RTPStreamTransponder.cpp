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


RTPStreamTransponder::RTPStreamTransponder(RTPIncomingSourceGroup* incoming, RTPReceiver* receiver, RTPOutgoingSourceGroup* outgoing,RTPSender* sender)
{
	//No thread
	setZeroThread(&thread);
	running = false;
	
	//Store streams
	this->incoming = incoming;
	this->outgoing = outgoing;
	this->receiver = receiver;
	this->sender = sender;
	
	//Add us as listeners
	incoming->AddListener(this);
	outgoing->AddListener(this);

	//Start
	Start();
	
	//Request update on the incoming
	if (this->receiver) this->receiver->SendPLI(this->incoming->media.ssrc);
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
	
	//Clone packet
	RTPPacket* cloned = packet->Clone();

	//Check if it is an VP9 packet
	if (cloned->GetCodec()==VideoCodec::VP9)
	{
		DWORD extSeqNum;
		bool mark;
		//Select layer
		if (!selector.Select(packet,extSeqNum,mark))
		       //Drop
		       return;
	       //Set them
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

		//Free mem
		delete(packet);
	}

	Log("<RTPBundleTransport::Run()\n");
	
	return 0;
}


