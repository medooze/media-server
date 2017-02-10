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
#include "rtpbuffer.h"

struct RTPSource 
{
	DWORD	ssrc;
	DWORD   extSeq;
	DWORD	cycles;
	DWORD	jitter;
	DWORD	numPackets;
	DWORD	numRTCPPackets;
	DWORD	totalBytes;
	DWORD	totalRTCPBytes;
	
	RTPSource()
	{
		ssrc		= random();
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
		ssrc		= random();
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
	DWORD	totalBytesSinceLastSR;
	DWORD	minExtSeqNumSinceLastSR ;
	DWORD   lostPacketsSinceLastSR;
	QWORD   lastReceivedSenderNTPTimestamp;
	QWORD   lastReceivedSenderReport;
	
	RTPIncomingSource() : RTPSource()
	{
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	virtual ~RTPIncomingSource()
	{
		
	}
	RTCPReport *CreateReport(QWORD now);
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
	
	virtual ~RTPOutgoingSource()
	{
		
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
	
	RTCPSenderReport* CreateSenderReport(timeval *tv) const;

	
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

class RTPOutgoingSourceGroup
{
public:
	class Listener 
	{
		public:
			virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) = 0;
		
	};
public:
	RTPOutgoingSourceGroup(MediaFrame::Type type,Listener* listener)
	{
		this->type = type;
		this->listener = listener;
	};
	RTPOutgoingSourceGroup(std::string &streamId,MediaFrame::Type type,Listener* listener)
	{
		this->streamId = streamId;
		this->type = type;
		this->listener = listener;
	};
public:
	typedef std::map<DWORD,RTPPacket*> RTPOrderedPackets;
public:	
	std::string streamId;
	MediaFrame::Type type;
	RTPOutgoingSource media;
	RTPOutgoingSource fec;
	RTPOutgoingSource rtx;
	RTPOrderedPackets	packets;
	Listener* listener;
};

struct RTPIncomingSourceGroup
{
public:
	class Listener 
	{
		public:
			virtual void onRTP(RTPIncomingSourceGroup* group,RTPPacket* packet) = 0;
	};
public:	
	RTPIncomingSourceGroup(MediaFrame::Type type,Listener* listener) : losts(64)
	{
		this->type = type;
		this->listener = listener;
		//Small bufer of 20ms
		packets.SetMaxWaitTime(20);
	};
	
	RTPIncomingSource* GetSource(DWORD ssrc)
	{
		if (ssrc == media.ssrc)
			return &media;
		else if (ssrc == rtx.ssrc)
			return &rtx;
		else if (ssrc == fec.ssrc)
			return &fec;
		return NULL;
	}
	int onRTP(DWORD ssrc,BYTE codec,BYTE* data,DWORD size);
public:	
	MediaFrame::Type type;
	RTPLostPackets	losts;
	RTPBuffer packets;
	RTPIncomingSource media;
	RTPIncomingSource fec;
	RTPIncomingSource rtx;
	Listener* listener;
};



#endif
