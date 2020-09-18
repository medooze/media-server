#ifndef RTPOUTGOINGSOURCE_H
#define RTPOUTGOINGSOURCE_H

#include "config.h"
#include "rtp/RTPSource.h"
#include "rtp/RTCPSenderReport.h"

struct RTPOutgoingSource : 
	public RTPSource
{
	bool	generatedSeqNum;
	DWORD   time;
	DWORD   seqGaps;
	DWORD   lastTime;
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
	
	Acumulator reportCountAcumulator;
	Acumulator reportedlostCountAcumulator;
	Acumulator reportedFractionLossAcumulator;
	
	RTPOutgoingSource();
	virtual ~RTPOutgoingSource() = default;
	
	DWORD CorrectExtSeqNum(DWORD extSeqNum);
	DWORD AddGapSeqNum();
	DWORD NextSeqNum();
	DWORD NextExtSeqNum();
	
	virtual void Reset() override;
	virtual void Update(QWORD now,DWORD seqNum,DWORD size) override;
	virtual void Update(QWORD now) override;
	
	RTCPSenderReport::shared CreateSenderReport(QWORD time);
	bool ProcessReceiverReport(QWORD time, const RTCPReport::shared& report);
	bool IsLastSenderReportNTP(DWORD ntp);
};

#endif /* RTPOUTGOINGSOURCE_H */

