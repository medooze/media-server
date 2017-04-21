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


RTPStreamTransponder::RTPStreamTransponder(RTPIncomingSourceGroup* incoming, RTPReceiver* receiver, RTPOutgoingSourceGroup* outgoing,RTPSender* sender)
{
	//Store streams
	this->incoming = incoming;
	this->outgoing = outgoing;
	this->receiver = receiver;
	this->sender = sender;

	//Add us as listeners
	incoming->AddListener(this);
	outgoing->AddListener(this);

	//Request update on the incoming
	if (this->receiver) this->receiver->SendPLI(this->incoming->media.ssrc);
}

RTPStreamTransponder::~RTPStreamTransponder()
{
	//Stop listeneing
	Close();
}

void RTPStreamTransponder::Close()
{
	ScopedLock lock(mutex);

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

	//Double check
	if (outgoing && sender)
	{
		//Change ssrc
		cloned->SetSSRC(outgoing->media.ssrc);
		//Send it on transport
		sender->Send(*cloned);
	}
	
	//Free mem
	delete(cloned);
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