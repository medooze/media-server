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
#include "WrapExtender.h"

class RTPStreamTransponder :
	public RTPIncomingMediaStream::Listener,
	public RTPOutgoingSourceGroup::Listener
{
public:
	static constexpr uint64_t NoFrameNum = std::numeric_limits<uint64_t>::max();
	static constexpr uint32_t NoSeqNum = std::numeric_limits<uint32_t>::max();
	static constexpr uint64_t NoTimestamp = std::numeric_limits<uint64_t>::max();
public:
	RTPStreamTransponder(const RTPOutgoingSourceGroup::shared& outgoing,const RTPSender::shared& sender);
	virtual ~RTPStreamTransponder();

	void ResetIncoming();
	void SetIncoming(const RTPIncomingMediaStream::shared& incoming, const RTPReceiver::shared& receiver, bool smooth = false);
	bool AppendH264ParameterSets(const std::string& sprop);
	void Close();

	// RTPIncomingMediaStream::Listener interface
	virtual void onRTP(const RTPIncomingMediaStream* stream, const RTPPacket::shared& packet) override;
	virtual void onBye(const RTPIncomingMediaStream* stream) override;
	virtual void onEnded(const RTPIncomingMediaStream* stream) override;


	// RTPOutgoingSourceGroup::Listener interface
	virtual void onPLIRequest(const RTPOutgoingSourceGroup* group,DWORD ssrc) override;
	virtual void onREMB(const RTPOutgoingSourceGroup* group, DWORD ssrc, DWORD bitrate) override;
	virtual void onEnded(const RTPOutgoingSourceGroup* group) override;

	void SelectLayer(int spatialLayerId, int temporalLayerId);
	void Mute(bool muting);
	void SetIntraOnlyForwarding(bool intraOnlyForwarding);

	const RTPIncomingMediaStream::shared GetIncoming() const { return incoming; }

protected:
	void RequestPLI();

private:
	TimeService& timeService;

	RTPOutgoingSourceGroup::shared  outgoing;
	RTPSender::shared		sender;
	RTPIncomingMediaStream::shared  incoming;
	RTPReceiver::shared		receiver;
	RTPIncomingMediaStream::shared  incomingNext;
	RTPReceiver::shared		receiverNext;

	std::unique_ptr<VideoLayerSelector> selector;

	volatile bool reset	= false;
	volatile bool muted	= false;
	DWORD firstExtSeqNum	= NoSeqNum;	//First seq num of incoming stream
	DWORD baseExtSeqNum	= 0;		//Base seq num of outgoing stream
	DWORD lastExtSeqNum	= 0;		//Last seq num of sent packet
	QWORD firstTimestamp	= NoTimestamp;  //First rtp timstamp of incoming stream
	QWORD baseTimestamp	= 0;		//Base rtp timestamp of ougogoing stream
	QWORD lastTimestamp	= 0;		//Last rtp timestamp of outgoing stream
	QWORD lastTime		= 0;		//Last sent time
	bool  lastCompleted	= true;		//Last packet enqueued had the M bit
	DWORD dropped		= 0;		//Num of empty packets dropped
	DWORD added		= 0;		//Number of added pacekts (like inserting h264 sps/pps)
	DWORD source		= 0;		//SSRC of the incoming rtp
	DWORD ssrc		= 0;		//SSRC to rewrite to
	MediaFrame::Type media;
	BYTE codec		= 0;
	BYTE type		= 0;
	volatile BYTE spatialLayerId		= LayerInfo::MaxLayerId;
	volatile BYTE temporalLayerId		= LayerInfo::MaxLayerId;
	volatile BYTE lastSpatialLayerId	= LayerInfo::MaxLayerId;

	std::optional<uint16_t> pictureId;
	std::optional<uint8_t>  temporalLevelZeroIndex;
	uint16_t lastSrcPictureId		= 0;
	uint8_t  lastSrcTemporalLevelZeroIndex	= 0;


	QWORD lastSentPLI			= 0;
	bool intraOnlyForwarding		= false;

	WrapExtender<uint16_t, uint64_t> frameNumberExtender;
	uint64_t firstFrameNumber	= NoFrameNum;
	uint64_t baseFrameNumber	= NoFrameNum;
	uint64_t lastFrameNumber	= NoFrameNum;

	RTPPacket::shared	h264Parameters;
	
};

#endif /* RTPSTREAMTRANSPONDER_H */
