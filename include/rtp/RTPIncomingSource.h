#ifndef RTPINCOMINGSOURCE_H
#define RTPINCOMINGSOURCE_H

#include <map>

#include "config.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPSource.h"
#include "rtp/RTCPReport.h"
#include "acumulator.h"
#include "WrapExtender.h"

struct RTPIncomingSource : public RTPSource
{
	DWORD	numFrames;
	DWORD	numFramesDelta;
	DWORD	lostPackets;
	DWORD	lostPacketsDelta;
	DWORD   lostPacketsGapCount;
	DWORD   lostPacketsMaxGap;
	DWORD	dropPackets;
	DWORD	totalPacketsSinceLastSR;
	DWORD	totalBytesSinceLastSR;
	DWORD	minExtSeqNumSinceLastSR ;
	DWORD   lostPacketsSinceLastSR;
	QWORD   lastReceivedSenderNTPTimestamp;
	QWORD   lastReceivedSenderTime;
	QWORD   lastReceivedSenderReport;
	QWORD   lastReport;
	QWORD	lastPLI;
	DWORD   totalPLIs;
	DWORD	totalNACKs;
	QWORD	lastNACKed;
	QWORD	lastTimestamp;
	QWORD	lastTime;
	QWORD   firstReceivedSenderTime;
	QWORD   firstReceivedSenderTimestamp;
	int64_t skew;
	double  drift;
	WrapExtender<uint32_t,uint64_t> timestampExtender;
	WrapExtender<uint32_t,uint64_t> lastReceivedSenderRTPTimestampExtender;
	std::map<WORD,LayerSource> layers;
	
	Acumulator acumulatorFrames;
	Acumulator acumulatorLostPackets;
	
	
	RTPIncomingSource();
	virtual ~RTPIncomingSource() = default;
	
	WORD  ExtendSeqNum(WORD seqNum);
	WORD  RecoverSeqNum(WORD osn);
	
	DWORD ExtendTimestamp(DWORD timestamp);
	DWORD RecoverTimestamp(DWORD timestamp);
	
	void Update(QWORD now,DWORD seqNum,DWORD size,const std::vector<LayerInfo> &layerInfos);
	
	void Process(QWORD now, const RTCPSenderReport::shared& sr);
	void SetLastTimestamp(QWORD now, QWORD timestamp);
	
	virtual void Update(QWORD now,DWORD seqNum,DWORD size) override;
	virtual void Update(QWORD now) override;
	virtual void Reset() override;
	
	void AddLostPackets(QWORD now, DWORD lost) 
	{
		lostPacketsDelta = acumulatorLostPackets.Update(now, lost); 
	}
	
	RTCPReport::shared CreateReport(QWORD now);
};

#endif /* RTPINCOMINGSOURCE_H */

