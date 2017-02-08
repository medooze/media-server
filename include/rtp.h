#ifndef _RTP_H_
#define _RTP_H_
#include <arpa/inet.h>
#include "config.h"
#include "log.h"
#include "media.h"
#include <vector>
#include <list>
#include <utility>
#include <map>
#include <math.h>

#include "rtp/RTPMap.h"
#include "rtp/RTPHeader.h"
#include "rtp/RTPHeaderExtension.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPPacketSched.h"
#include "rtp/RTPRedundantPacket.h"
#include "rtp/RTPDepacketizer.h"
#include "rtp/RTCPReport.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCompoundPacket.h"
#include "rtp/RTCPApp.h"
#include "rtp/RTCPExtendedJitterReport.h"
#include "rtp/RTCPSenderReport.h"
#include "rtp/RTCPBye.h"
#include "rtp/RTCPFullIntraRequest.h"
#include "rtp/RTCPPayloadFeedback.h"
#include "rtp/RTCPRTPFeedback.h"
#include "rtp/RTCPNACK.h"
#include "rtp/RTCPReceiverReport.h"
#include "rtp/RTCPSDES.h"

struct RTPSource 
{
	DWORD	SSRC;
	DWORD   extSeq;
	DWORD	cycles;
	DWORD	jitter;
	DWORD	numPackets;
	DWORD	numRTCPPackets;
	DWORD	totalBytes;
	DWORD	totalRTCPBytes;
	
	RTPSource()
	{
		SSRC		= random();
		extSeq		= random();
		cycles		= 0;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
		jitter		= 0;
	}
	
	RTCPCompoundPacket* CreateSenderReport();
	
	virtual void Reset()
	{
		SSRC		= random();
		extSeq		= random();
		cycles		= 0;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
		jitter		= 0;
	}
};

struct RTPIncomingSource : public RTPSource
{
	DWORD	lostPackets;
	DWORD	totalPacketsSinceLastSR;
	DWORD   nackedPacketsSinceLastSR;
	DWORD	totalBytesSinceLastSR;
	DWORD	minExtSeqNumSinceLastSR ;
	DWORD   lostPacketsSinceLastSR;
	
	RTPIncomingSource() : RTPSource()
	{
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		nackedPacketsSinceLastSR = 0;
		totalBytesSinceLastSR	 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		nackedPacketsSinceLastSR = 0;
		totalBytesSinceLastSR	 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
};

struct RTPOutgoingSource : public RTPSource
{
	DWORD   time;
	DWORD   lastTime;
	DWORD	numPackets;
	DWORD	numRTCPPackets;
	DWORD	totalBytes;
	DWORD	totalRTCPBytes;
	
	RTPOutgoingSource() : RTPSource()
	{
		time		= random();
		lastTime	= time;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
	}
	
	RTCPSenderReport* CreateSenderReport(timeval *tv)
	{
		//Create Sender report
		RTCPSenderReport *sr = new RTCPSenderReport();

		//Append data
		sr->SetSSRC(SSRC);
		sr->SetTimestamp(tv);
		sr->SetRtpTimestamp(lastTime);
		sr->SetOctectsSent(totalBytes);
		sr->SetPacketsSent(numPackets);
		
		//Return it
		return sr;
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		time		= random();
		lastTime	= time;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
	}
	
};

struct RTPIncomingRtxSource : public RTPIncomingSource
{
	int apt;
	RTPIncomingSource* original;

	RTPIncomingRtxSource() : RTPIncomingSource()
	{
		apt = -1;
		original = NULL;
	}
};

struct RTPOutgoingRtxSource : public RTPOutgoingSource
{
	int apt;
	RTPIncomingSource* original;

	RTPOutgoingRtxSource() : RTPOutgoingSource()
	{
		apt = -1;
		original = NULL;
	}
};

class RTPLostPackets
{
public:
	RTPLostPackets(WORD num);
	~RTPLostPackets();
	void Reset();
	WORD AddPacket(const RTPPacket *packet);
	std::list<RTCPRTPFeedback::NACKField*>  GetNacks();
	void Dump();
	DWORD GetTotal() {return total;}
	
private:
	QWORD *packets;
	WORD size;
	WORD len;
	DWORD first;
	DWORD total;
};

#endif
