#ifndef RTPOUTGOINGSOURCE_H
#define RTPOUTGOINGSOURCE_H

#include "config.h"
#include "rtp/RTPSource.h"
#include "rtp/RTCPSenderReport.h"

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
	
	virtual ~RTPOutgoingSource() = default;
	
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

#endif /* RTPOUTGOINGSOURCE_H */

