/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPStreamTransponder.h
 * Author: Sergio
 *
 * Created on 20 de abril de 2017, 18:33
 */

#ifndef RTPSTREAMTRANSPONDER_H
#define RTPSTREAMTRANSPONDER_H

#include <queue>
#include "rtp.h"
#include "waitqueue.h"
#include "VideoLayerSelector.h"

class RTPStreamTransponder : 
	public RTPIncomingSourceGroup::Listener,
	public RTPOutgoingSourceGroup::Listener
{
public:
	RTPStreamTransponder(RTPOutgoingSourceGroup* outgoing,RTPSender* sender);
	virtual ~RTPStreamTransponder();
	
	bool SetIncoming(RTPIncomingSourceGroup* incoming, RTPReceiver* receiver);
	void Close();
	
	virtual void onRTP(RTPIncomingSourceGroup* group,RTPPacket* packet) override;
	virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) override;
	
	void SelectLayer(int spatialLayerId,int temporalLayerId);
protected:
	void Start();
	int Run();
	void Stop();
	void Reset();
	void RequestPLI();
private:
	static void * run(void *par);

private:
	
	RTPOutgoingSourceGroup *outgoing;
	RTPIncomingSourceGroup *incoming;
	RTPReceiver* receiver;
	RTPSender* sender;
	VideoLayerSelector* selector;
	Mutex mutex;
	WaitCondition wait;
	std::queue<RTPPacket*> packets;
	pthread_t thread;
	bool running;
	DWORD first;	//First seq num of incoming stream
	DWORD base;	//Last outgoing seq num when first was set
	DWORD last;	//Last seq num of sent packet
	DWORD dropped;  //Num of empty packets dropped
	DWORD ssrc;	//SSRC to rewrite to
	BYTE spatialLayerId;
	BYTE temporalLayerId;
};

#endif /* RTPSTREAMTRANSPONDER_H */

