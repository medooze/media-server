#ifndef RTPOUTGOINGSOURCE_H
#define RTPOUTGOINGSOURCE_H

#include "config.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPSource.h"
#include "rtp/RTCPSenderReport.h"

struct RTPOutgoingSource : 
	public RTPSource
{
	bool	generatedSeqNum;
	DWORD   time;
	DWORD   seqGaps;
	DWORD	numFrames;
	DWORD	numFramesDelta;
	DWORD   lastTimestamp;
	DWORD   lastPayloadType;
	QWORD	lastSenderReport;
	QWORD	lastSenderReportNTP;
	DWORD	remb;
	DWORD	reportCount;
	DWORD	reportCountDelta;
	DWORD	reportedLostCount;
	DWORD	reportedLostCountDelta;
	BYTE	reportedFractionLost;
	DWORD	reportedJitter;
	DWORD	rtt;
	
	Acumulator<uint32_t, uint64_t> acumulatorFrames;
	Acumulator<uint32_t, uint64_t> reportCountAcumulator;
	Acumulator<uint32_t, uint64_t> reportedlostCountAcumulator;
	Acumulator<uint32_t, uint64_t> reportedFractionLossAcumulator;
	
	RTPOutgoingSource();
	virtual ~RTPOutgoingSource() = default;
	
	DWORD CorrectExtSeqNum(DWORD extSeqNum);
	DWORD AddGapSeqNum();
	DWORD NextSeqNum();
	DWORD NextExtSeqNum();

	void Update(QWORD now, const RTPPacket::shared& packet, DWORD size);
	void Update(QWORD now, const RTPHeader& header, DWORD size);
	
	
	virtual void Reset() override;
	virtual void Update(QWORD now,DWORD seqNum,DWORD size,DWORD overheadSize) override;
	virtual void Update(QWORD now) override;
	
	RTCPSenderReport::shared CreateSenderReport(QWORD time);
	bool ProcessReceiverReport(QWORD time, const RTCPReport::shared& report);
	bool IsLastSenderReportNTP(DWORD ntp);

	void SetLastTimestamp(QWORD now, QWORD timestamp);
};

#endif /* RTPOUTGOINGSOURCE_H */

