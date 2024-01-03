#include "rtp/RTPSource.h"

RTPSource::RTPSource() :
	acumulator(1E3, 1E3, 1E3),
	acumulatorTotalBitrate(1E3, 1E3, 1E3),
	acumulatorPackets(1E3, 1E3, 1E3)
{

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

void RTPSource::Update(QWORD now, DWORD seqNum,DWORD size,DWORD overheadSize) 
{
	//Calculate total size
	DWORD totalSize = size + overheadSize;
	
	//Increase stats
	numPackets++;
	totalBytes += totalSize;

	//Update bitrate acumulators and get value in bps
	bitrate = acumulator.Update(now,size) * 8;
	totalBitrate = acumulatorTotalBitrate.Update(now, totalSize) * 8;
	
	//Update packets acumulator
	numPacketsDelta = acumulatorPackets.Update(now,1);
}

void RTPSource::Update(QWORD now) 
{
	//Update bitrate acumulators and get value in bps
	bitrate = acumulator.Update(now) * 8;
	totalBitrate = acumulatorTotalBitrate.Update(now) * 8;
	//Update packets acumulator
	numPacketsDelta = acumulatorPackets.Update(now);
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
	totalBitrate	= 0;
	clockrate	= 0;
	//Reset accumulators
	acumulator.Reset(0);
	acumulatorTotalBitrate.Reset(0);
	acumulatorPackets.Reset(0);
}
