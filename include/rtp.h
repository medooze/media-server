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
#include <set>
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
	virtual ~RTPSource()
	{
		
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
	QWORD   lastReport;
	
	RTPIncomingSource() : RTPSource()
	{
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lostPacketsSinceLastSR   = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		lastReport		 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lostPacketsSinceLastSR   = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		lastReport		 = 0;
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
	QWORD	lastSenderReport;
	QWORD	lastSenderReportNTP;
	
	RTPOutgoingSource() : RTPSource()
	{
		time		= random();
		lastTime	= time;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
		lastSenderReport	= 0;
		lastSenderReportNTP	= 0;
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
	
	RTCPSenderReport* CreateSenderReport(QWORD time);

	bool IsLastSenderReportNTP(DWORD ntp)
	{
		return  
			//Check last send SR 32 middle bits
			((lastSenderReportNTP << 16 | lastSenderReportNTP >> 16) == ntp)
			//Check last 16bits of each word to match cisco bug
			|| ((lastSenderReportNTP << 16 | (lastSenderReportNTP | 0xFFFF)) == ntp);
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

class RTPOutgoingSourceGroup
{
public:
	class Listener 
	{
		public:
			virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) = 0;
		
	};
public:
	RTPOutgoingSourceGroup(MediaFrame::Type type)
	{
		this->type = type;
	};
	
	RTPOutgoingSourceGroup(std::string &streamId,MediaFrame::Type type)
	{
		this->streamId = streamId;
		this->type = type;
	};
	
	void AddListener(Listener* listener) 
	{
		ScopedLock scoped(mutex);
		listeners.insert(listener);
	}
	
	void RemoveListener(Listener* listener) 
	{
		ScopedLock scoped(mutex);
		listeners.insert(listener);
	}
	
	void onPLIRequest(DWORD ssrc)
	{
		ScopedLock scoped(mutex);
		for (Listeners::const_iterator it=listeners.begin();it!=listeners.end();++it)
			(*it)->onPLIRequest(this,ssrc);
	}
public:
	typedef std::map<DWORD,RTPPacket*> RTPOrderedPackets;
	typedef std::set<Listener*> Listeners;
public:	
	std::string streamId;
	MediaFrame::Type type;
	RTPOutgoingSource media;
	RTPOutgoingSource fec;
	RTPOutgoingSource rtx;
	RTPOrderedPackets	packets;
	Mutex mutex;
	Listeners listeners;
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
	
	RTPIncomingSourceGroup(MediaFrame::Type type) : losts(64)
	{
		this->type = type;
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
	
	void AddListener(Listener* listener) 
	{
		ScopedLock scoped(mutex);
		listeners.insert(listener);
	}
	
	void RemoveListener(Listener* listener) 
	{
		ScopedLock scoped(mutex);
		listeners.insert(listener);
	}
	
	void onRTP(RTPPacket* packet)
	{
		ScopedLock scoped(mutex);
		for (Listeners::const_iterator it=listeners.begin();it!=listeners.end();++it)
			(*it)->onRTP(this,packet);
		delete(packet);
	}
public:
	typedef std::set<Listener*> Listeners;
public:	
	MediaFrame::Type type;
	RTPLostPackets	losts;
	RTPBuffer packets;
	RTPIncomingSource media;
	RTPIncomingSource fec;
	RTPIncomingSource rtx;
	Mutex mutex;
	Listeners listeners;
};


class RTPSender
{
public:
		
	virtual int Send(RTPPacket &packet) = 0;
};

class RTPReceiver
{
public:
	virtual int SendPLI(DWORD ssrc) = 0;
};
#endif
