#include "rtp/RTPOutgoingSource.h"


RTCPSenderReport::shared RTPOutgoingSource::CreateSenderReport(QWORD now)
{
	//Create Sender report
	auto sr = std::make_shared<RTCPSenderReport>();

	//Append data
	sr->SetSSRC(ssrc);
	sr->SetTimestamp(now);
	sr->SetRtpTimestamp(lastTime);
	sr->SetOctectsSent(totalBytes);
	sr->SetPacketsSent(numPackets);
	
	//Store last sending time
	lastSenderReport = now;
	//Store last send SR 32 middle bits
	lastSenderReportNTP = sr->GetNTPTimestamp();
	
	//Return it
	return sr;
}
