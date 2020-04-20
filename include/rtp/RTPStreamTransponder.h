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
#include "VideoLayerSelector.h"

class RTPStreamTransponder : 
	public RTPIncomingMediaStream::Listener,
	public RTPOutgoingSourceGroup::Listener
{
public:
	RTPStreamTransponder(RTPOutgoingSourceGroup* outgoing,RTPSender* sender);
	virtual ~RTPStreamTransponder();
	
	bool SetIncoming(RTPIncomingMediaStream* incoming, RTPReceiver* receiver);
	bool AppendH264ParameterSets(const std::string& sprop);
	void Close();
	
	virtual void onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet) override;
	virtual void onBye(RTPIncomingMediaStream* stream) override;
	virtual void onEnded(RTPIncomingMediaStream* stream) override;
	virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) override;
	virtual void onREMB(RTPOutgoingSourceGroup* group,DWORD ssrc,DWORD bitrate) override;
	
	void SelectLayer(int spatialLayerId,int temporalLayerId);
	void Mute(bool muting);
protected:
	void RequestPLI();

private:
	
	RTPOutgoingSourceGroup *outgoing	= NULL;
	RTPIncomingMediaStream *incoming	= NULL;
	RTPReceiver* receiver			= NULL;
	RTPSender* sender			= NULL;
	std::unique_ptr<VideoLayerSelector> selector;
	Mutex mutex;
	
	
	volatile bool reset	= false;
	volatile bool muted	= false;
	DWORD firstExtSeqNum	= 0;  //First seq num of incoming stream
	DWORD baseExtSeqNum	= 0;  //Base seq num of outgoing stream
	DWORD lastExtSeqNum	= 0;  //Last seq num of sent packet
	QWORD firstTimestamp	= 0;  //First rtp timstamp of incoming stream
	QWORD baseTimestamp	= 0;  //Base rtp timestamp of ougogoing stream
	QWORD lastTimestamp	= 0;  //Last rtp timestamp of outgoing stream
	QWORD lastTime		= 0;  //Last sent time
	bool  lastCompleted	= true; //Last packet enqueued had the M bit
	DWORD dropped		= 0;  //Num of empty packets dropped
	DWORD added		= 0;
	DWORD source		= 0;  //SSRC of the incoming rtp
	DWORD ssrc		= 0;  //SSRC to rewrite to
	MediaFrame::Type media;
	BYTE codec		= 0;
	BYTE type		= 0;
	volatile BYTE spatialLayerId		= LayerInfo::MaxLayerId;
	volatile BYTE temporalLayerId		= LayerInfo::MaxLayerId;
	volatile BYTE lastSpatialLayerId	= LayerInfo::MaxLayerId;
	WORD lastPicId		= 0;
	WORD lastTl0Idx		= 0;
	QWORD picId		= 0;
	WORD tl0Idx		= 0;
	bool rewritePicId	= true;
	QWORD lastSentPLI	= 0;
	
	RTPPacket::shared	h264Parameters;
};

#endif /* RTPSTREAMTRANSPONDER_H */
