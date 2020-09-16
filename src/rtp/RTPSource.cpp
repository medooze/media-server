#include "rtp/RTPSource.h"

RTPSource::RTPSource() : acumulator(1000),acumulatorPackets(1000)
{
	ssrc		= 0;
	extSeqNum	= 0;
	cycles		= 0;
	numPackets	= 0;
	numPacketsDelta = 0;	
	numRTCPPackets	= 0;
	totalBytes	= 0;
	totalRTCPBytes	= 0;
	jitter		= 0;
	bitrate		= 0;
	clockrate	= 0;
}

WORD RTPSource::SetSeqNum(WORD seqNum)
{
	//If first
	if (!numPackets)
	{
		//Update seq num
		this->extSeqNum = seqNum;
		//Done
		return cycles;
	}
	
	//Check if we have a sequence wrap
	if (seqNum<0x0FFF && (this->extSeqNum & 0xFFFF)>0xF000)
		//Increase cycles
		cycles++;

	//Get ext seq
	DWORD extSeqNum = ((DWORD)cycles)<<16 | seqNum;

	//If it is an out of order packet from previous cycle
	if (seqNum>0xF000 && (this->extSeqNum & 0xFFFF)<0x0FFF)
		//Do nothing and return prev one
		return cycles-1;

	//If we have a not out of order pacekt
	if (extSeqNum > this->extSeqNum)
		//Update seq num
		this->extSeqNum = extSeqNum;

	//Return seq cycles count
	return cycles;
}

void RTPSource::SetExtSeqNum(DWORD extSeqNum )
{
	//Updte seqNum and cycles
	this->extSeqNum = extSeqNum;
	cycles = extSeqNum >>16;

}

void RTPSource::Update(QWORD now, DWORD seqNum,DWORD size) 
{
	//Increase stats
	numPackets++;
	totalBytes += size;

	//Update bitrate acumulator
	acumulator.Update(now,size);
	acumulatorPackets.Update(now,1);

	//Get bitrate in bps
	bitrate = acumulator.GetInstant()*8;
}

void RTPSource::Update(QWORD now) 
{
	//Update bitrate acumulator
	acumulator.Update(now);

	//Get bitrate in bps
	bitrate = acumulator.GetInstant()*8;
	//Get num packets in window
	numPacketsDelta = acumulatorPackets.GetInstant();
}

void RTPSource::Reset()
{
	extSeqNum	= 0;
	cycles		= 0;
	numPackets	= 0;
	numPacketsDelta = 0;
	numRTCPPackets	= 0;
	totalBytes	= 0;
	totalRTCPBytes	= 0;
	jitter		= 0;
	bitrate		= 0;
	clockrate	= 0;
	//Reset accumulators
	acumulator.Reset(0);
}
