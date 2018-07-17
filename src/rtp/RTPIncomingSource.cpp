#include "rtp/RTPIncomingSource.h"

RTCPReport::shared RTPIncomingSource::CreateReport(QWORD now)
{
	//If we have received somthing
	if (!totalPacketsSinceLastSR || !(extSeq>=minExtSeqNumSinceLastSR))
		//Nothing to report
		return NULL;
	
	//Get number of total packtes
	DWORD total = extSeq - minExtSeqNumSinceLastSR + 1;
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
	report->SetLastSeqNum(extSeq);

	//Reset data
	lastReport = now;
	totalPacketsSinceLastSR = 0;
	totalBytesSinceLastSR = 0;
	minExtSeqNumSinceLastSR = RTPPacket::MaxExtSeqNum;

	//Return it
	return report;
}