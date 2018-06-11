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
	
	virtual void onRTP(RTPIncomingSourceGroup* group,const RTPPacket::shared& packet) override;
	virtual void onEnded(RTPIncomingSourceGroup* group) override;
	virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) override;
	virtual void onREMB(RTPOutgoingSourceGroup* group,DWORD ssrc,DWORD bitrate) override;
	
	void SelectLayer(int spatialLayerId,int temporalLayerId);
	void Mute(bool muting);
protected:
	//TODO: I don't like it
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
	std::queue<RTPPacket::shared> packets;
	pthread_t thread	= {0};
	bool running		= false;;
	bool muted		= false;
	DWORD firstExtSeqNum	= 0;  //First seq num of incoming stream
	DWORD baseExtSeqNum	= 0;  //Base seq num of outgoing stream
	DWORD lastExtSeqNum	= 0;  //Last seq num of sent packet
	DWORD firstTimestamp	= 0;  //First rtp timstamp of incoming stream
	QWORD baseTimestamp	= 0;  //Base rtp timestamp of ougogoing stream
	QWORD lastTimestamp	= 0;  //Last rtp timestamp of outgoing stream
	QWORD lastTime		= 0;  //Last sent time
	DWORD dropped		= 0;  //Num of empty packets dropped
	DWORD ssrc		= 0;  //SSRC to rewrite to
	BYTE spatialLayerId	= LayerInfo::MaxLayerId;
	BYTE temporalLayerId	= LayerInfo::MaxLayerId;
	WORD lastPicId		= 0;
	WORD lastTl0Idx		= 0;
	QWORD picId		= 0;
	WORD tl0Idx		= 0;
	bool rewritePicId	= true;
};

#endif /* RTPSTREAMTRANSPONDER_H */
