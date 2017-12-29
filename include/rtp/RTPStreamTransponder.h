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
	
	RTPOutgoingSourceGroup *outgoing	= NULL;
	RTPIncomingSourceGroup *incoming	= NULL;
	RTPReceiver* receiver			= NULL;
	RTPSender* sender			= NULL;
	VideoLayerSelector* selector		= NULL;
	Mutex mutex;
	WaitCondition wait;
	std::queue<RTPPacket*> packets;
	pthread_t thread	= {0};
	bool running		= false;;
	DWORD first		= 0 ;	//First seq num of incoming stream
	DWORD base		= 0;	//Last outgoing seq num when first was set
	DWORD last		= 0;	//Last seq num of sent packet
	DWORD dropped		= 0;  //Num of empty packets dropped
	DWORD ssrc		= 0;	//SSRC to rewrite to
	BYTE spatialLayerId	= VideoLayerSelector::MaxLayerId;
	BYTE temporalLayerId	= VideoLayerSelector::MaxLayerId;
};

#endif /* RTPSTREAMTRANSPONDER_H */
