#include "EventSource.h"

EvenSource::EvenSource(){}
EvenSource::EvenSource(const char* str){}
EvenSource::EvenSource(const std::wstring &str){}
EvenSource::~EvenSource(){}
void EvenSource::SendEvent(const char* type,const char* msg,...){}


#include "config.h"
#include "rtp.h"
#include "PCAPReader.h"
#include "SendSideBandwidthEstimation.h"

struct Stat
{
	uint32_t num = 0;
	uint64_t sent = 0;
	uint64_t recv = 0;
	uint32_t fb = 0;
};

int main(int argc, char** argv)
{
	Logger::EnableDebug(true);
	Logger::EnableUltraDebug(true);
	
	uint64_t time;
	RTPMap extMap;
	PCAPReader reader;
	SendSideBandwidthEstimation senderSideBandwidthEstimator;
	
	extMap[1] = RTPHeaderExtension::SSRCAudioLevel;
	//extMap[5] = RTPHeaderExtension::TransportWideCC;
	extMap[2] = RTPHeaderExtension::TransportWideCC;
	
	reader.Open(argv[1]);
	
	while(time=reader.Next())
	{
		const auto data = reader.GetUDPData();
		const auto size = reader.GetUDPSize();
		
		//Check if it is a local packet
		bool local = reader.GetOriginIp() == 0x7F000001;
		
		//Check it is not RTCP
		if (RTCPCompoundPacket::IsRTCP(data,size))
		{
			//If not an incoming rtcp
			if (local)
				//Ignore
				continue;
			
			//Parse it
			auto rtcp = RTCPCompoundPacket::Parse(data,size);

			//Check packet
			if (!rtcp)
			{
				//Debug
				Debug("-DTLSICETransport::onData() | RTCP wrong data\n");
				//Dump it
				::Dump(data,size);
				//Exit
				return 1;
			}
			
			//For each packet
			for (DWORD i = 0; i<rtcp->GetPacketCount();i++)
			{
				//Get pacekt
				auto packet = rtcp->GetPacket(i);
				
				//We need an feedback packet
				if (packet->GetType()==RTCPPacket::RTPFeedback)
				{
					//Get feedback packet
					auto fb = std::static_pointer_cast<RTCPRTPFeedback>(packet);
					
					//We need a transport wide feedback message
					if (fb->GetFeedbackType()==RTCPRTPFeedback::TransportWideFeedbackMessage)
					{
						//Get each fiedl
						for (DWORD i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							auto field = fb->GetField<RTCPRTPFeedback::TransportWideFeedbackMessageField>(i);
							//Pass it to the estimator
							senderSideBandwidthEstimator.ReceivedFeedback(field->feedbackPacketCount,field->packets, time);
						}
					}
				}
			}
			
		} else {
			
			//If not an outgoing rtp
			if (!local)
				//Ignore
				continue;
			
			RTPHeader header;
			RTPHeaderExtension extension;

			//Parse RTP header
			uint32_t len = header.Parse(data,size);
			//On error
			if (!len)
			{
				//Debug
				Error("-PCAPTransportEmulator::Run() | Could not parse RTP header ini=%u len=%d\n",len,size-len);
				//Ignore this try again
				continue;
			}
			//If it has extension
			if (!header.extension)
				//Skip
				continue;
			
			//Parse extension
			if(!extension.Parse(extMap,data+len,size-len))
			{
				///Debug
				Error("-PCAPTransportEmulator::Run() | Could not parse RTP header extension ini=%u len=%d\n",len,size-len);
				//retry
				continue;
			}
			//Get seq num
			if (!extension.hasTransportWideCC)
				continue;
			
			//Create stat
			auto stats = PacketStats::Create(
				extension.transportSeqNum,
				header.ssrc,
				header.sequenceNumber,
				size,
				0, //TODO: calculate effective payload, i.e. no padding or rtx
				header.timestamp,
				time,
				header.mark
			);
			//Add new stat
			senderSideBandwidthEstimator.SentPacket(stats);
		}
		
	}
	reader.Close();
	
	/*
	uint64_t firstSent = 0;
	uint64_t firstRecv = 0;
	uint64_t prevSent = 0;
	uint64_t prevRecv = 0;
	
	for (auto& entry : packets)
	{
		auto& stat = entry.second;
		if (!firstSent)
			firstSent = stat.sent;
		if (!firstRecv)
			firstRecv = stat.recv;
		//Correc ts
		auto sent = stat.sent - firstSent;
		auto recv = stat.recv - firstRecv;
		
		//Get deltas
		auto deltaSent = sent - prevSent;
		auto deltaRecv = recv - prevRecv;
		auto delta = deltaRecv - deltaSent;
		
		printf("#%u sent:%.8lu (+%.6lu) recv:%.8lu (+%.6lu) delta:%.6ld fb:%u, first sent:%llu, sent:%llu\n",stat.num,sent,deltaSent,recv,deltaRecv,delta,stat.fb,firstSent,stat.sent);
		
		prevSent = sent;
		prevRecv = recv;
		
	}
	*/
	
	return 0;
}

