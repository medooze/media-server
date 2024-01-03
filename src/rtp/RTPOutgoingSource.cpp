#include "rtp/RTPOutgoingSource.h"

RTPOutgoingSource::RTPOutgoingSource() : 
	RTPSource(),
	acumulatorFrames(1000),
	reportCountAcumulator(1000),
	reportedlostCountAcumulator(1000),
	reportedFractionLossAcumulator(1000)
{
	time			= random();
	numFrames		= 0;
	numFramesDelta		= 0;
	lastTimestamp		= 0;
	ssrc			= random();
	extSeqNum		= random() & 0xFFFF;
	lastSenderReport	= 0;
	lastSenderReportNTP	= 0;
	lastPayloadType		= 0xFF;
	seqGaps			= 0;
	generatedSeqNum		= false;
	remb			= 0;
	reportCount		= 0;
	reportCountDelta	= 0;
	reportedLostCount	= 0;
	reportedLostCountDelta	= 0;
	reportedFractionLost	= 0;
	reportedJitter		= 0;
	rtt			= 0;
}

DWORD RTPOutgoingSource::CorrectExtSeqNum(DWORD extSeqNum) 
{
	//Updte sequence number with introduced gaps
	DWORD corrected = extSeqNum + seqGaps;
	//Set last seq num
	SetExtSeqNum(corrected);
	//return corrected 
	return corrected;
}

DWORD RTPOutgoingSource::AddGapSeqNum()
{
	//Add gap
	seqGaps++;
	//Add seq num
	SetExtSeqNum(++extSeqNum);
	//Add one more
	return extSeqNum;
}

DWORD RTPOutgoingSource::NextSeqNum()
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

DWORD RTPOutgoingSource::NextExtSeqNum()
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

void RTPOutgoingSource::Reset()
{
	RTPSource::Reset();
	ssrc			= random();
	extSeqNum		= random() & 0xFFFF;
	time			= random();
	numFrames		= 0;
	numFramesDelta		= 0;
	lastTimestamp		= 0;
	lastSenderReport	= 0;
	lastSenderReportNTP	= 0;
	lastPayloadType		= 0xFF;
	seqGaps			= 0;
	generatedSeqNum		= false;
	remb			= 0;
	reportCount		= 0;
	reportCountDelta	= 0;
	reportedLostCount	= 0;
	reportedLostCountDelta	= 0;
	reportedFractionLost	= 0;
	reportedJitter		= 0;
	rtt			= 0;
}

void RTPOutgoingSource::Update(QWORD now,DWORD seqNum,DWORD size,DWORD overheadSize)
{
	RTPSource::Update(now,seqNum,size,overheadSize);

	//If not auotgenerated
	if (!generatedSeqNum)
		//Update seq num
		SetSeqNum(seqNum);
}
void RTPOutgoingSource::Update(QWORD now)
{
	RTPSource::Update(now);
	//Set deltas
	numFramesDelta		= acumulatorFrames.Update(now);
	reportCountDelta	= reportCountAcumulator.Update(now);
	reportedLostCountDelta	= reportedlostCountAcumulator.Update(now);
	
	//Get fraction loss average
	reportedFractionLossAcumulator.Update(now);
	reportedFractionLost = static_cast<BYTE>(reportedFractionLossAcumulator.GetInstantMedia());
}

bool RTPOutgoingSource::IsLastSenderReportNTP(DWORD ntp)
{
	return  ntp && (
		//Check last send SR 32 middle bits
		((DWORD)((lastSenderReportNTP >> 16) & 0xFFFFFFFF) == ntp)
		//Check last 16bits of each word to match cisco bug
		|| ((lastSenderReportNTP << 16 | (lastSenderReportNTP | 0xFFFF)) == ntp)
	);
}

RTCPSenderReport::shared RTPOutgoingSource::CreateSenderReport(QWORD now)
{
	//Create Sender report
	auto sr = std::make_shared<RTCPSenderReport>();

	//Append data
	sr->SetSSRC(ssrc);
	//TODO: lastTime?
	sr->SetTimestamp(now);
	sr->SetRtpTimestamp(lastTimestamp);
	sr->SetOctectsSent(totalBytes);
	sr->SetPacketsSent(numPackets);
	
	//Store last sending time
	lastSenderReport = now;
	//Store last send SR 32 middle bits
	lastSenderReportNTP = sr->GetNTPTimestamp();

	//Return it
	return sr;
}

bool RTPOutgoingSource::ProcessReceiverReport(QWORD now, const RTCPReport::shared& report)
{
	//Increate report count
	reportCount++;
	reportCountDelta = reportCountAcumulator.Update(now, 1);
	
	//Increase lost counter
	DWORD lostCount = report->GetLostCount();
	reportedLostCount += lostCount;
	reportedLostCountDelta = reportedlostCountAcumulator.Update(now, lostCount);
	
	//Get fraction loss
	reportedFractionLossAcumulator.Update(now, report->GetFactionLost());
	
	//Get jitter
	reportedJitter	=  report->GetJitter();
	
	//Calculate RTT
	if (!IsLastSenderReportNTP(report->GetLastSR()))
		//Rtt not updated
		return false;
	
	//Calculate new rtt in ms
	rtt = now - lastSenderReport/1000-report->GetDelaySinceLastSRMilis();
	
	//RTT updated
	return true;
	
}

void RTPOutgoingSource::Update(QWORD now, const RTPPacket::shared& packet, DWORD size)
{
	//Set clockrate
	clockrate = packet->GetClockRate();
	//Update from headers
	Update(now, packet->GetRTPHeader(), size);
}

void RTPOutgoingSource::Update(QWORD now, const RTPHeader& header, DWORD size)
{
	//Update last send time
	SetLastTimestamp(now,header.timestamp);
	//Update current payload
	lastPayloadType = header.payloadType;

	//Update stats
	Update(now, header.sequenceNumber, size, header.GetSize());
}

void RTPOutgoingSource::SetLastTimestamp(QWORD now, QWORD timestamp)
{
	//If new packet is newer
	if (timestamp > lastTimestamp)
	{
		//One new frame
		numFrames++;
		numFramesDelta = acumulatorFrames.Update(now, 1);
		//Store last time
		lastTimestamp = timestamp;
	}
	//lastTime = now;
}
