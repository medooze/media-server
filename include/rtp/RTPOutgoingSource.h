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
	
	RTPOutgoingSource() : RTPSource()
	{
		time			= random();
		lastTime		= time;
		ssrc			= random();
		extSeqNum		= random() & 0xFFFF;
		lastSenderReport	= 0;
		lastSenderReportNTP	= 0;
		lastPayloadType		= 0xFF;
		seqGaps			= 0;
		generatedSeqNum		= false;
		remb			= 0;
	}
	
	DWORD CorrectExtSeqNum(DWORD extSeqNum) 
	{
		//Updte sequence number with introduced gaps
		DWORD corrected = extSeqNum + seqGaps;
		//Set last seq num
		SetExtSeqNum(corrected);
		//return corrected 
		return corrected;
	}
	
	DWORD AddGapSeqNum()
	{
		//Add gap
		seqGaps++;
		//Add seq num
		SetExtSeqNum(++extSeqNum);
		//Add one more
		return extSeqNum;
	}
	
	DWORD NextSeqNum()
	{
		//We are generating the seq nums
		generatedSeqNum = true;
		
		//Get next
		DWORD next = (++extSeqNum) & 0xFFFF;
		
		//Check if we have a sequence wrap
		if (!next)
			//Increase cycles
			cycles++;

		//Return it
		return next;
	}
	
	DWORD NextExtSeqNum()
	{
		//We are generating the seq nums
		generatedSeqNum = true;
		
		//Get next
		DWORD next = (++extSeqNum) & 0xFFFF;
		
		//Check if we have a sequence wrap
		if (!next)
			//Increase cycles
			cycles++;

		//Return it
		return extSeqNum;
	}
	
	virtual ~RTPOutgoingSource() = default;
	
	virtual void Reset()
	{
		RTPSource::Reset();
		ssrc		= random();
		extSeqNum	= random() & 0xFFFF;
		time		= random();
		lastTime	= time;
		lastPayloadType	= 0xFF;
		remb		= 0;
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
		return  ntp && (
			//Check last send SR 32 middle bits
			((DWORD)((lastSenderReportNTP >> 16) & 0xFFFFFFFF) == ntp)
			//Check last 16bits of each word to match cisco bug
			|| ((lastSenderReportNTP << 16 | (lastSenderReportNTP | 0xFFFF)) == ntp)
		);
	}
};

#endif /* RTPOUTGOINGSOURCE_H */

