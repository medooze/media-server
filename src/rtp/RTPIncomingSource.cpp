#include "rtp/RTPIncomingSource.h"

RTCPReport::shared RTPIncomingSource::CreateReport(QWORD now)
{
	//If we have received somthing
	if (!totalPacketsSinceLastSR || !(extSeqNum>=minExtSeqNumSinceLastSR))
		//Nothing to report
		return NULL;
	
	//Get number of total packtes
	DWORD total = extSeqNum - minExtSeqNumSinceLastSR + 1;
	//Calculate lost
	DWORD lostPacketsSinceLastSR = total - totalPacketsSinceLastSR;
	//Add to total lost count
	lostPackets += lostPacketsSinceLastSR;
	//Calculate fraction lost
	DWORD frac = (lostPacketsSinceLastSR*256)/total;

	//Calculate time since last report
	DWORD delaySinceLastSenderReport = 0;
	//Double check it is correct 
	if (lastReceivedSenderReport && now > lastReceivedSenderReport)
		//Get diff in ms
		delaySinceLastSenderReport = (now - lastReceivedSenderReport)/1000;
	//Create report
	RTCPReport::shared report = std::make_shared<RTCPReport>();

	//Set SSRC of incoming rtp stream
	report->SetSSRC(ssrc);

	//Get time and update it
	report->SetDelaySinceLastSRMilis(delaySinceLastSenderReport);
	// The middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) 
	// received as part of the most recent RTCP sender report (SR) packet from source SSRC_n.
	// If no SR has been received yet, the field is set to zero.
	//Other data
	report->SetLastSR(lastReceivedSenderNTPTimestamp >> 16);
	report->SetFractionLost(frac);
	report->SetLastJitter(jitter);
	report->SetLostCount(lostPackets);
	report->SetLastSeqNum(extSeqNum);

	//Reset data
	lastReport = now;
	totalPacketsSinceLastSR = 0;
	totalBytesSinceLastSR = 0;
	minExtSeqNumSinceLastSR = RTPPacket::MaxExtSeqNum;

	//Return it
	return report;
}

RTPIncomingSource::RTPIncomingSource() : 
	RTPSource(),
	acumulatorFrames(1000),
	acumulatorLostPackets(1000)
{
	numFrames			= 0;
	numFramesDelta			= 0;
	lostPackets			= 0;
	lostPacketsDelta		= 0;
	lostPacketsGapCount		= 0;
	lostPacketsMaxGap		= 0;
	dropPackets			= 0;
	totalPacketsSinceLastSR		= 0;
	totalBytesSinceLastSR		= 0;
	lostPacketsSinceLastSR		= 0;
	lastReceivedSenderNTPTimestamp	= 0;
	lastReceivedSenderTime		= 0;
	lastReceivedSenderReport	= 0;
	lastReport			= 0;
	lastPLI				= 0;
	totalPLIs			= 0;
	totalNACKs			= 0;
	lastNACKed			= 0;
	lastTimestamp			= 0;
	lastTime			= 0;
	firstReceivedSenderTime		= 0;
	firstReceivedSenderTimestamp	= 0;
	skew				= 0;
	drift				= 1;
	minExtSeqNumSinceLastSR		= RTPPacket::MaxExtSeqNum;
}


void RTPIncomingSource::Reset()
{
	RTPSource::Reset();
	numFrames			= 0;
	numFramesDelta			= 0;
	lostPackets			= 0;
	lostPacketsDelta		= 0;
	lostPacketsGapCount		= 0;
	lostPacketsMaxGap		= 0;	
	dropPackets			= 0;
	totalPacketsSinceLastSR		= 0;
	totalBytesSinceLastSR		= 0;
	lostPacketsSinceLastSR		= 0;
	lastReceivedSenderNTPTimestamp  = 0;
	lastReceivedSenderTime		= 0;
	lastReceivedSenderReport	= 0;
	lastReport			= 0;
	lastPLI				= 0;
	totalPLIs			= 0;
	lastNACKed			= 0;
	lastTimestamp			= 0;
	lastTime			= 0;
	firstReceivedSenderTime		= 0;
	firstReceivedSenderTimestamp	= 0;
	skew				= 0;
	drift				= 1;
	minExtSeqNumSinceLastSR		= RTPPacket::MaxExtSeqNum;
	timestampExtender.Reset();
	lastReceivedSenderRTPTimestampExtender.Reset();
}

DWORD RTPIncomingSource::ExtendTimestamp(DWORD timestamp)
{
	//Set timestamp
	return timestampExtender.Extend(timestamp);
}

WORD RTPIncomingSource::ExtendSeqNum(WORD seqNum)
{
	//Update seq num
	auto cycles = SetSeqNum(seqNum);
	//Check if it is the min for this SR
	if (extSeqNum<minExtSeqNumSinceLastSR)
		//Store minimum
		minExtSeqNumSinceLastSR = extSeqNum;
	//OK
	return cycles; 
}

void RTPIncomingSource::Update(QWORD now,DWORD seqNum,DWORD size,const std::vector<LayerInfo> &layerInfos)
{
	//Update source normally
	RTPIncomingSource::Update(now,seqNum,size);
	//For each layer
	for (const auto& layerInfo : layerInfos)
	{
		//Check layer info is present
		if (layerInfo.IsValid())
		{
			//Insert layer info if it doesn't exist
			auto [it, inserted] = layers.try_emplace(layerInfo.GetId(), layerInfo);
			//Update layer source
			it->second.Update(now,size);
		}
	}
}

void RTPIncomingSource::Update(QWORD now,DWORD seqNum,DWORD size)
{
	//Store last seq number before updating
	//DWORD lastExtSeqNum = extSeqNum;

	//Update source
	RTPSource::Update(now,seqNum,size);

	totalPacketsSinceLastSR++;
	totalBytesSinceLastSR += size;
	
	//TODO: remove, this should be redundant
	SetSeqNum(seqNum);

	
	/*TODO: calculate jitter
	//If we have a not out of order pacekt
	if (lastExtSeqNum != extSeqNum)
	{
		//If it is not first one and not from the same frame
		if (lastTimestamp && lastTimestamp<timestamp)
		{
			//Get diff from previous
			QWORD diff = (lastTime-now)/1000;


			//Get difference of arravail times
			int d = (timestamp-lastTimestamp)-diff;
			//Check negative
			if (d<0)
				//Calc abs
				d = -d;
			//Calculate variance
			int v = d - jitter;
			//Calculate jitter
			jitter += v/16;
		}
		//Update rtp timestamp
		lastTime = timestamp;
	}
	 */
}

void RTPIncomingSource::Update(QWORD now)
{
	//Update source normally
	RTPSource::Update(now);
	//Update also all media layers
	for (auto& [layerId,layer] : layers)
		//Update bitrate also
		layer.Update(now);
	//Update deltas
	numFramesDelta	 = acumulatorFrames.Update(now);
	lostPacketsDelta = acumulatorLostPackets.Update(now);
	lostPacketsGapCount = acumulatorLostPackets.GetCount();
	lostPacketsMaxGap = acumulatorLostPackets.GetMaxValueInWindow();
}

void RTPIncomingSource::Process(QWORD now, const RTCPSenderReport::shared& sr)
{
	//If first
	if (!firstReceivedSenderTime)
	{
		//Store time
		firstReceivedSenderTime = sr->GetTimestamp()/1000;
		firstReceivedSenderTimestamp = sr->GetRTPTimestamp();
	}
	//Store info
	lastReceivedSenderNTPTimestamp = sr->GetNTPTimestamp();
	lastReceivedSenderTime = sr->GetTimestamp()/1000;
	lastReceivedSenderRTPTimestampExtender.Extend(sr->GetRTPTimestamp());
	lastReceivedSenderReport = now;
	
	//Ensure we have clock rate configured
	if (clockrate)
	{
		//Get diff in sender time
		QWORD deltaTime = (lastReceivedSenderTime-firstReceivedSenderTime);
		QWORD deltaTimestamp = (lastReceivedSenderRTPTimestampExtender.GetExtSeqNum()-firstReceivedSenderTimestamp)*1000/clockrate;
		//Calculate skew
		skew = deltaTime - deltaTimestamp;
		drift = deltaTime ? (double)deltaTimestamp/deltaTime : 1;
		//Debug
		UltraDebug("-RTPIncomingSource::Process() [ssrc:0x%x,skew:%lld,deltaTime:%llu,deltaTimestamp:%llu,senderTime:%llu,clockrate:%u]\n",ssrc,skew,deltaTime,deltaTimestamp,lastReceivedSenderTime,clockrate);
	}
}

/*
 * Get seq num cycles from a past sequence numer
 */
WORD RTPIncomingSource::RecoverSeqNum(WORD osn)
{
	 //Check secuence wrap
	if ((extSeqNum & 0xFFFF)<0x0FFF && (osn>0xF000))
		//It is from the past cycle
		return cycles - 1;
	//It is from current cyle
	return cycles;
}

/*
 * Get seq num cycles from a past sequence numer
 */
DWORD RTPIncomingSource::RecoverTimestamp(DWORD timestamp)
{
	return timestampExtender.RecoverCycles(timestamp);
}

void RTPIncomingSource::SetLastTimestamp(QWORD now, QWORD timestamp)
{
	//If new packet is newer
	if (timestamp>lastTimestamp)
	{
		//One new frame
		numFrames++;
		numFramesDelta = acumulatorFrames.Update(now, 1);
		//Store last time
		lastTimestamp = timestamp;
	}
	lastTime = now;
}
