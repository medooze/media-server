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
#include "acumulator.h"
#include "use.h"


struct LayerInfo
{
	static BYTE MaxLayerId; 
	BYTE temporalLayerId = MaxLayerId;
	BYTE spatialLayerId  = MaxLayerId;
	
	bool IsValid() { return spatialLayerId!=MaxLayerId || temporalLayerId != MaxLayerId;	}
	WORD GetId()   { return ((WORD)spatialLayerId)<<8  | temporalLayerId;			}
};

struct LayerSource : LayerInfo
{
	DWORD		numPackets = 0;
	DWORD		totalBytes = 0;
	Acumulator	bitrate;
	
	LayerSource() : bitrate(1000)
	{
		
	}
	
	LayerSource(const LayerInfo& layerInfo) : bitrate(1000)
	{
		spatialLayerId  = layerInfo.spatialLayerId;
		temporalLayerId = layerInfo.temporalLayerId; 
	}
	
	void Update(QWORD now, DWORD size) 
	{
		//Increase stats
		numPackets++;
		totalBytes += size;
		
		//Update bitrate acumulator
		bitrate.Update(now,size);
	}
};

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
	Acumulator bitrate;
	
	RTPSource() : bitrate(1000)
	{
		ssrc		= 0;
		extSeq		= 0;
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
	
	RTCPCompoundPacket::shared CreateSenderReport();
	
	void SetSeqNum(WORD seqNum)
	{
		//Check if we have a sequence wrap
		if (seqNum<0x0FFF && (extSeq & 0xFFFF)>0xF000)
			//Increase cycles
			cycles++;

		//Get ext seq
		DWORD extSeq = ((DWORD)cycles)<<16 | seqNum;

		//If we have a not out of order pacekt
		if (extSeq > this->extSeq || !numPackets)
			//Update seq num
			this->extSeq = extSeq;
	}
	
	virtual void Update(QWORD now, DWORD seqNum,DWORD size) 
	{
		//Increase stats
		numPackets++;
		totalBytes += size;
		
		//Update bitrate acumulator
		bitrate.Update(now,size);
	}
	
	virtual void Reset()
	{
		ssrc		= 0;
		extSeq		= 0;
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
	DWORD	dropPackets;
	DWORD	totalPacketsSinceLastSR;
	DWORD	totalBytesSinceLastSR;
	DWORD	minExtSeqNumSinceLastSR ;
	DWORD   lostPacketsSinceLastSR;
	QWORD   lastReceivedSenderNTPTimestamp;
	QWORD   lastReceivedSenderReport;
	QWORD   lastReport;
	QWORD	lastPLI;
	DWORD   totalPLIs;
	DWORD	totalNACKs;
	QWORD	lastNACKed;
	std::map<WORD,LayerSource> layers;
	
	RTPIncomingSource() : RTPSource()
	{
		lostPackets		 = 0;
		dropPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lostPacketsSinceLastSR   = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		lastReport		 = 0;
		lastPLI			 = 0;
		totalPLIs		 = 0;
		totalNACKs		 = 0;
		lastNACKed		 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	
	void Update(QWORD now,DWORD seqNum,DWORD size, LayerInfo layerInfo)
	{
		//Update source normally
		RTPIncomingSource::Update(now,seqNum,size);
		//Check layer info is present
		if (layerInfo.IsValid())
		{
			//Find layer
			auto it = layers.find(layerInfo.GetId());
			//If not found
			if (it==layers.end())
				//Add new entry
				it = layers.emplace(layerInfo.GetId(),LayerSource(layerInfo)).first;
			//Update layer source
			it->second.Update(now,size);
		}
	}
	
	virtual void Update(QWORD now,DWORD seqNum,DWORD size)
	{
		RTPSource::Update(now,seqNum,size);
		
		//Update seq num
		SetSeqNum(seqNum);
		
		totalPacketsSinceLastSR++;
		totalBytesSinceLastSR += size;

		//Check if it is the min for this SR
		if (extSeq<minExtSeqNumSinceLastSR)
			//Store minimum
			minExtSeqNumSinceLastSR = extSeq;
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		lostPackets		 = 0;
		dropPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		totalBytesSinceLastSR	 = 0;
		lostPacketsSinceLastSR   = 0;
		lastReceivedSenderNTPTimestamp = 0;
		lastReceivedSenderReport = 0;
		lastReport		 = 0;
		lastPLI			 = 0;
		totalPLIs		 = 0;
		lastNACKed		 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	virtual ~RTPIncomingSource() = default;
	
	RTCPReport::shared CreateReport(QWORD now);
};

struct RTPOutgoingSource : public RTPSource
{
	bool	generatedSeqNum;
	DWORD   time;
	DWORD   lastTime;
	QWORD	lastSenderReport;
	QWORD	lastSenderReportNTP;
	
	RTPOutgoingSource() : RTPSource()
	{
		time			= random();
		lastTime		= time;
		ssrc			= random();
		extSeq			= random() & 0xFFFF;
		lastSenderReport	= 0;
		lastSenderReportNTP	= 0;
		generatedSeqNum		= false;
	}
	
	DWORD NextSeqNum()
	{
		//We are generating the seq nums
		generatedSeqNum = true;
		
		//Get next
		DWORD next = (++extSeq) & 0xFFFF;
		
		//Check if we have a sequence wrap
		if (!next)
			//Increase cycles
			cycles++;

		//Return it
		return next;
	}
	
	virtual ~RTPOutgoingSource()
	{
		
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		ssrc		= random();
		extSeq		= random() & 0xFFFF;
		time		= random();
		lastTime	= time;
	}
	
	virtual void Update(QWORD now,DWORD seqNum,DWORD size)
	{
		RTPSource::Update(now,seqNum,size);
	
		//If not auotgenerated
		if (!generatedSeqNum)
			//Update seq num
			SetSeqNum(seqNum);
	}
	
	RTCPSenderReport::shared CreateSenderReport(QWORD time);

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
	WORD AddPacket(const RTPPacket::shared &packet);
	std::list<RTCPRTPFeedback::NACKField::shared>  GetNacks() const;
	void Dump() const;
	DWORD GetTotal() const {return total;}
	
private:
	QWORD *packets;
	WORD size;
	WORD len;
	DWORD first;
	DWORD total;
};

struct RTPOutgoingSourceGroup
{
public:
	class Listener 
	{
		public:
			virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) = 0;
		
	};
public:
	RTPOutgoingSourceGroup(MediaFrame::Type type);
	RTPOutgoingSourceGroup(std::string &streamId,MediaFrame::Type type);
	
	void AddListener(Listener* listener);
	void RemoveListener(Listener* listener);
	void onPLIRequest(DWORD ssrc);
	
	RTPOutgoingSource* GetSource(DWORD ssrc);
	
	//RTX packets
	void AddPacket(const RTPPacket::shared& packet);
	RTPPacket::shared GetPacket(WORD seq) const;
	void ReleasePackets(QWORD until);
	
public:	
	std::string streamId;
	MediaFrame::Type type;
	RTPOutgoingSource media;
	RTPOutgoingSource fec;
	RTPOutgoingSource rtx;
private:	
	mutable Mutex mutex;
	std::map<DWORD,RTPPacket::shared> packets;
	std::set<Listener*> listeners;
};

struct RTPIncomingSourceGroup
{
public:
	class Listener 
	{
	public:
		virtual void onRTP(RTPIncomingSourceGroup* group,const RTPPacket::shared& packet) = 0;
		virtual void onEnded(RTPIncomingSourceGroup* group) = 0;
	};
public:	
	RTPIncomingSourceGroup(MediaFrame::Type type);
	~RTPIncomingSourceGroup();
	
	RTPIncomingSource* GetSource(DWORD ssrc);
	void AddListener(Listener* listener);
	void RemoveListener(Listener* listener);
	int AddPacket(const RTPPacket::shared &packet);
	void ResetPackets();
	void Update(QWORD now);
	void SetRTT(DWORD rtt);
	std::list<RTCPRTPFeedback::NACKField::shared>  GetNacks() { return losts.GetNacks(); }
	
	void Start();
	void Stop();
	
	DWORD GetCurrentLost()		const { return losts.GetTotal();		}
	DWORD GetMinWaitedTime()	const { return packets.GetMinWaitedime();	}
	DWORD GetMaxWaitedTime()	const { return packets.GetMaxWaitedTime();	}
	long double GetAvgWaitedTime()	const {	return packets.GetAvgWaitedTime();	}
	
public:	
	std::string rid;
	std::string mid;
	DWORD rtt;
	WORD  rttrtxSeq;
	QWORD rttrtxTime;
	MediaFrame::Type type;
	RTPIncomingSource media;
	RTPIncomingSource fec;
	RTPIncomingSource rtx;
private:
	pthread_t dispatchThread = {0};
	RTPLostPackets	losts;
	RTPBuffer packets;
	Mutex mutex;
	std::set<Listener*>  listeners;
};

class RTPSender
{
public:
	virtual int Send(const RTPPacket::shared& packet) = 0;
};

class RTPReceiver
{
public:
	virtual int SendPLI(DWORD ssrc) = 0;
};
#endif
