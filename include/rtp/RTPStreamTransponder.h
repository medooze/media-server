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

#include "rtp.h"
#include "vp9/VP9LayerSelector.h"

class RTPStreamTransponder : 
	public RTPIncomingSourceGroup::Listener,
	public RTPOutgoingSourceGroup::Listener
{
public:
	RTPStreamTransponder(RTPIncomingSourceGroup* incoming, RTPReceiver* receiver, RTPOutgoingSourceGroup* outgoing,RTPSender* sender);
	virtual ~RTPStreamTransponder();
	void Close();
	
	virtual void onRTP(RTPIncomingSourceGroup* group,RTPPacket* packet) override;
	virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) override;
	
	void SelectLayer(int spatialLayerId,int temporalLayerId);
	
private:
	RTPOutgoingSourceGroup *outgoing;
	RTPIncomingSourceGroup *incoming;
	RTPReceiver* receiver;
	RTPSender* sender;
	VP9LayerSelector selector;
	Mutex mutex;
};

#endif /* RTPSTREAMTRANSPONDER_H */

