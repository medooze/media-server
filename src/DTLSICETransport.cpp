/* 
 * File:   RTPICETransport.cpp
 * Author: Sergio
 * 
 * Created on 8 de enero de 2017, 18:37
 */
#include "tracing.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <srtp2/srtp.h>
#include <time.h>
#include "log.h"
#include "assertions.h"
#include "tools.h"
#include "codecs.h"
#include "rtp.h"
#include "stunmessage.h"
#include <openssl/opensslconf.h>
#include <openssl/ossl_typ.h>
#include <queue>
#include <algorithm>
#include "DTLSICETransport.h"
#include "PCAPFile.h"
#include "rtp/RTPMap.h"
#include "rtp/RTPHeader.h"
#include "rtp/RTPHeaderExtension.h"
#include "EventLoop.h"
#include "Endpoint.h"
#include "VideoLayerSelector.h"
#include <algorithm>

constexpr auto IceTimeout			= 30000ms;
constexpr auto ProbingInterval			= 5ms;
constexpr auto MaxRTXOverhead			= 0.70f;
constexpr auto TransportWideCCMaxPackets	= 100;
constexpr auto TransportWideCCMaxInterval	= 5E4;	//50ms
constexpr auto MaxProbingHistorySize		= 50;
constexpr auto RtxRttThresholdMs 		= 300;

DTLSICETransport::DTLSICETransport(Sender *sender,TimeService& timeService, ObjectPool<Packet>& packetPool) :
	sender(sender),
	timeService(timeService),
	packetPool(packetPool),
	endpoint(timeService),
	dtls(*this,timeService,endpoint.GetTransport()),
	history(MaxProbingHistorySize, false),
	outgoingBitrate(250, 1E3, 250),
	rtxBitrate(250, 1E3, 250),
	probingBitrate(250, 1E3, 250),
	senderSideBandwidthEstimator(new SendSideBandwidthEstimation())
{
	Debug(">DTLSICETransport::DTLSICETransport() [this:%p]\n", this);
}

DTLSICETransport::~DTLSICETransport()
{
	Debug(">DTLSICETransport::~DTLSICETransport() [this:%p,started:%d\n", this, started);
	
	//Stop
	Stop();
}

void DTLSICETransport::onDTLSPendingData()
{
	//UltraDebug(">DTLSConnection::onDTLSPendingData() [active:%p]\n",active);
	//Until depleted
	while(active)
	{
		//Pick one packet from the pool
		Packet buffer = packetPool.pick();

		//Read from dtls
		int len = dtls.Read(buffer.GetData(),buffer.GetCapacity());
		//Check result
		if (len<=0)
		{
			//Return packet to pool
			packetPool.release(std::move(buffer));
			break;
		}
		//Set read size
		buffer.SetSize(len);
		//Send
		//UltraDebug("-DTLSConnection::onDTLSPendingData() | dtls send [len:%d]\n",len);
		//Send it back
		sender->Send(active,std::move(buffer));
		//Update bitrate
		outgoingBitrate.Update(getTimeMS(),len);
	}
	//UltraDebug("<DTLSConnection::onDTLSPendingData() | no more data\n");
}

int DTLSICETransport::onData(const ICERemoteCandidate* candidate,const BYTE* data,DWORD size)
{
	TRACE_EVENT("transport", "DTLSICETransport::onData", "size", size);

	//Get current time
	auto now = getTime();

	//Check if it a DTLS packet
	if (DTLSConnection::IsDTLS(data,size))
	{
		//Feed it
		dtls.Write(data,size);
		//Exit
		return 1;
	}

	//Check if it is RTCP
	if (RTCPCompoundPacket::IsRTCP(data,size))
	{
		TRACE_EVENT("rtp", "DTLSICETransport::onData::RTCP", "size", size);

		//Check session
		if (!recv.IsSetup())
			return Warning("-DTLSICETransport::onData() |  Recv SRTPSession is not setup\n");

		//unprotect
		size_t len = recv.UnprotectRTCP((BYTE*)data,size);
		
		//Check size
		if (!len)
			//Error
			return Warning("-DTLSICETransport::onData() | Error unprotecting rtcp packet [%s]\n",recv.GetLastError());

		//If dumping
		if (dumper && dumpRTCP)
			//Write udp packet
			dumper->WriteUDP(now/1000,candidate->GetIPAddress(),candidate->GetPort(),0x7F000001,5004,data,len);

		//Parse it
		auto rtcp = RTCPCompoundPacket::Parse(data,len);

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

		//Process it
		this->onRTCP(rtcp);

		//Skip
		return 1;
	}

	//Check session
	if (!recv.IsSetup())
		return Warning("-DTLSICETransport::onData() | Recv SRTPSession is not setup\n");
	
	//unprotect
	size_t len = recv.UnprotectRTP((BYTE*)data,size);
	//Check status
	if (!len)
		//Error
		return Warning("-DTLSICETransport::onData() | Error unprotecting rtp packet [%s]\n",recv.GetLastError());
	
	//Parse rtp packet
	RTPPacket::shared packet = RTPPacket::Parse(data,len,recvMaps.rtp,recvMaps.ext,now/1000);
	
	//Check
	if (!packet)
		//Error
		return Warning("-DTLSICETransport::onData() | Could not parse rtp packet\n");

	TRACE_EVENT("rtp", "DTLSICETransport::onData::RTP",
		"ssrc", packet->GetSSRC(),
		"seqnum", packet->GetSeqNum(),
		"pt", packet->GetPayloadType(),
		"payload", packet->GetMediaLength(),
		"act", packet->GetAbsoluteCaptureTime());
	
	//If dumping
	if (dumper && dumpInRTP)
	{
		//Get truncate size
		DWORD truncate = dumpRTPHeadersOnly ? len - packet->GetMediaLength() + 16 : 0;
		//Write udp packet
		dumper->WriteUDP(now/1000,candidate->GetIPAddress(),candidate->GetPort(),0x7F000001,5004,data,len, truncate);
	}

	//Get ssrc
	DWORD ssrc = packet->GetSSRC();

	//If transport wide cc is used, process now as it can be padding only data not associated yet with any source used for bwe
	if (packet->HasTransportWideCC())
	{
		//Store the last received ssrc for media to be signaled via twcc
		lastMediaSSRC = ssrc;

		// Get current seq mum
		WORD transportSeqNum = packet->GetTransportSeqNum();

		//Get max seq num so far, it is either last one if queue is empy or last one of the queue
		DWORD maxFeedbackPacketExtSeqNum = transportWideReceivedPacketsStats.size() ? transportWideReceivedPacketsStats.rbegin()->first : lastFeedbackPacketExtSeqNum;

		//Check if we have a sequence wrap
		if (transportSeqNum < 0x00FF && (maxFeedbackPacketExtSeqNum & 0xFFFF)>0xFF00)
		{
			//Increase cycles
			feedbackCycles++;
			//If looping
			if (feedbackCycles)
				//Send feedback now
				SendTransportWideFeedbackMessage(ssrc);
		}

		//Get extended value
		DWORD transportExtSeqNum = feedbackCycles << 16 | transportSeqNum;

		//Add packets to the transport wide stats
		transportWideReceivedPacketsStats[transportExtSeqNum] = PacketStats::Create(packet, size, now);

		//If we have enought or timeout 
		if (packet->GetMark() || transportWideReceivedPacketsStats.size() > TransportWideCCMaxPackets || (now - transportWideReceivedPacketsStats.begin()->second.time) > TransportWideCCMaxInterval)
			//Send feedback message
			SendTransportWideFeedbackMessage(ssrc);
		//Schedule for later
		if (transportWideReceivedPacketsStats.size())
		{
			//If timer is still valid and has not been scheduled already
			if (sseTimer && !sseTimer->IsScheduled())
				//Schedule
				sseTimer->Reschedule(std::chrono::milliseconds((int)(TransportWideCCMaxInterval/1000)), 0ms);
		//If timer is still valid and still scheduled
		} else if (sseTimer && sseTimer->IsScheduled()) {
			//Cancel it
			sseTimer->Cancel();
		}
	}
	
	//Get codec
	DWORD codec = packet->GetCodec();
	
	//Check codec
	if (codec==RTPMap::NotFound)
		//Exit
		return Warning("-DTLSICETransport::onData() | RTP packet payload type unknown [%d]\n",packet->GetPayloadType());
	
	//Get group
	RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);

	//If it doesn't have a group
	if (!group)
	{
		//Get rid
		auto mid = packet->GetMediaStreamId();
		auto rid = packet->HasRepairedId() ? packet->GetRepairedId() : packet->GetRId();

		Debug("-DTLSICETransport::onData() | Unknowing group for ssrc trying to retrieve by [ssrc:%u,rid:'%s']\n",ssrc,rid.c_str());

		//If it is the repaidr stream or it has rid and it is rtx
		if (!rid.empty() && (packet->GetCodec()==VideoCodec::RTX || packet->GetCodec() == AudioCodec::RTX))
		{
			//Try to find it on the rids and mids
			auto it = rids.find(mid+"@"+rid);
			//If found
			if (it!=rids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtx stream to ssrc [ssrc:%u,mid:'%s',rid:'%s']\n",ssrc,mid.c_str(),rid.c_str());

				//Got source
				group = it->second;

				//Check if there was a previous ssrc
				if (group->rtx.ssrc)
				{
					//Remove previous one
					incoming.erase(group->rtx.ssrc);
					//Also from srtp session
					recv.RemoveStream(group->rtx.ssrc);
				}

				//Set ssrc for next ones
				group->rtx.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
				//And to the srtp session
				recv.AddStream(ssrc);
			}
		} else if (packet->HasRId()) {
			//Try to find it on the rids and mids
			auto it = rids.find(mid+"@"+rid);
			//If found
			if (it!=rids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtp stream to ssrc [ssrc:%u,mid:'%s',rid:'%s']\n",ssrc,mid.c_str(),rid.c_str());

				//Got source
				group = it->second;

				//Check if there was a previous ssrc
				if (group->media.ssrc)
				{
					//Remove previous one
					incoming.erase(group->media.ssrc);
					//Also from srtp session
					recv.RemoveStream(group->media.ssrc);
				}

				//Set ssrc for next ones
				group->media.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
				//And to the srtp session
				recv.AddStream(ssrc);
			}
		} else if (packet->HasMediaStreamId() && (packet->GetCodec()==VideoCodec::RTX || packet->GetCodec() == AudioCodec::RTX)) {
			//Try to find it on the rids and mids
			auto it = mids.find(mid);
			//If found
			if (it!=mids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtx stream id to ssrc [ssrc:%u,mid:'%s']\n",ssrc,mid.c_str());

				//Get first source in set, if there was more it should have contained an rid
				group = *it->second.begin();

				//Check if there was a previous ssrc
				if (group->rtx.ssrc)
				{
					//Remove previous one
					incoming.erase(group->rtx.ssrc);
					//Also from srtp session
					recv.RemoveStream(group->rtx.ssrc);
				}

				//Set ssrc for next ones
				group->rtx.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
				//And to the srtp session
				recv.AddStream(ssrc);
			}
		} else if (packet->HasMediaStreamId()) {
			//Try to find it on the rids and mids
			auto it = mids.find(mid);
			//If found
			if (it!=mids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtp stream to ssrc [ssrc:%u,mid:'%s']\n",ssrc,mid.c_str());

				//Get first source in set, if there was more it should have contained an rid
				group = *it->second.begin();

				//Check if there was a previous ssrc
				if (group->media.ssrc)
				{
					//Remove previous one
					incoming.erase(group->media.ssrc);
					//Also from srtp session
					recv.RemoveStream(group->media.ssrc);
				}

				//Set ssrc for next ones
				group->media.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
				//And to the srtp session
				recv.AddStream(ssrc);
			}
		}
	}
			
	//Ensure it has a group
	if (!group)	
		//error
		return Warning("-DTLSICETransport::onData() | Unknowing group for ssrc [%u]\n",ssrc);
	
	//Assing mid if found
	if (group->mid.empty() && packet->HasMediaStreamId())
	{
		//Get mid
		auto mid = packet->GetMediaStreamId();
		//Debug
		Log("-DTLSICETransport::onData() | Assinging media stream id [ssrc:%u,mid:'%s']\n",ssrc,mid.c_str());
		//Set it
		group->mid = mid;
		//Find mid 
		auto it = mids.find(mid);
		//If not there
		if (it!=mids.end())
			//Append
			it->second.insert(group);
		else
			//Add new set
			mids[mid] = { group };
	}
	
	//UltraDebug("-DTLSICETransport::onData() | Got RTP on media:%s sssrc:%u seq:%u pt:%u codec:%s rid:'%s', mid:'%s'\n",MediaFrame::TypeToString(group->type),ssrc,packet->GetSeqNum(),packet->GetPayloadType(),GetNameForCodec(group->type,codec),group->rid.c_str(),group->mid.c_str());
	
	//Process packet and get source
	RTPIncomingSource* source = group->Process(packet);
	
	//Ensure it has a source
	if (!source)
		//error
		return Warning("-DTLSICETransport::onData() | Group does not contain ssrc [%u]\n",ssrc);

	//If it was an RTX packet and not a padding only one
	if (ssrc==group->rtx.ssrc && packet->GetMediaLength()) 
	{
		//Ensure that it is a RTX codec
		if (codec!=VideoCodec::RTX && codec != AudioCodec::RTX)
			//error
			return  Warning("-DTLSICETransport::onData() | No RTX codec on rtx sssrc:%u type:%d codec:%d\n",packet->GetSSRC(),packet->GetPayloadType(),packet->GetCodec());
		//Find apt type
		auto apt = recvMaps.apt.GetCodecForType(packet->GetPayloadType());
		//Find codec 
		codec = recvMaps.rtp.GetCodecForType(apt);
		//Check codec
		if (codec==RTPMap::NotFound)
			  //Error
			  return Warning("-DTLSICETransport::onData() | RTP RTX packet apt type unknown [%s %d]\n",MediaFrame::TypeToString(packet->GetMediaType()),packet->GetPayloadType());
		
		//Remove OSN and restore seq num
		if (!packet->RecoverOSN())
			//Error
			return Warning("-DTLSICETransport::onData() | RTP Could not recoever OSX\n");
		
		 //Set original ssrc
		 packet->SetSSRC(group->media.ssrc);
		 //Set corrected seq num cycles
		 packet->SetSeqCycles(group->media.RecoverSeqNum(packet->GetSeqNum()));
		 //Set corrected timestamp cycles
		 packet->SetTimestampCycles(group->media.RecoverTimestamp(packet->GetTimestamp()));
		 //Set codec
		 packet->SetCodec(codec);
		 packet->SetPayloadType(apt);
		 //TODO: Move from here, required to fill the vp8/vp9 descriptors
		 VideoLayerSelector::GetLayerIds(packet);

		 //UltraDebug("-DTLSICETransport::onData() | Recovered RTX on media:%s sssrc:%u seq:%u pt:%u codec:%s rid:'%s', mid:'%s'\n", MediaFrame::TypeToString(group->type), ssrc, packet->GetSeqNum(), packet->GetPayloadType(), GetNameForCodec(group->type, codec), group->rid.c_str(), group->mid.c_str());
	} 

	//Add packet and see if we have lost any in between
	int lost = group->AddPacket(packet,size,now/1000);

	//Check if it was rejected
	if (lost<0)
		//Increase rejected counter
		source->dropPackets++;
	
	//Send nack feedback
	if ((group->type == MediaFrame::Video || recvMaps.apt.GetTypeForCodec(packet->GetPayloadType()!=RTPMap::NotFound)) &&
		( lost>0 ||  (group->GetCurrentLost() && (now-source->lastNACKed)/1000>std::max(rtt,20u)))
	   )
	{
		//UltraDebug("-DTLSICETransport::onData() | Lost packets [ssrc:%u,ssrc:%u,seq:%d,lost:%d,total:%u]\n",ssrc,packet->GetSSRC(),packet->GetSeqNum(),lost,group->GetCurrentLost());

		//Create rtcp sender retpor
		auto rtcp = RTCPCompoundPacket::Create();

		//Create NACK
		auto nack = rtcp->CreatePacket<RTCPRTPFeedback>(RTCPRTPFeedback::NACK,mainSSRC,packet->GetSSRC());

		//Get nacks for lost
		for (auto field : group->GetNacks())
			//Add it
			nack->AddField(field);
		//Send packet
		Send(rtcp);

		//Update last time nacked
		source->lastNACKed = now;
		//Update nacked packets
		source->totalNACKs++;
	}
	
	//Check if we need to send RR (1 per second)
	if (now-source->lastReport>1E6)
	{
		//Create rtcp sender retpor
		auto rtcp = RTCPCompoundPacket::Create();
		
		//Create receiver report for normal stream
		auto rr = rtcp->CreatePacket<RTCPReceiverReport>(mainSSRC);
		
		//Create report
		auto report = source->CreateReport(now);

		//If got anything
		if (report)
			//Append it
			rr->AddReport(report);
		
		//If we are using remb and have a value
		if (overrideBWE || group->remoteBitrateEstimation)
		{
			
			//Add remb block
			DWORD bitrate = 0;
			std::list<DWORD> ssrcs;
			
			if (!overrideBWE)
			{
				//If we haave a mid in group
				if (!group->mid.empty())
				{
					//for each group of this mid
					for (const auto& other : mids[group->mid])
					{
						//Append
						ssrcs.push_back(other->media.ssrc);
						bitrate += other->remoteBitrateEstimation;
					}
				} else {
					//Just this group
					ssrcs.push_back(group->media.ssrc);
					bitrate = group->remoteBitrateEstimation;
				}
			} else {
				bitrate = remoteOverrideBitrate;
			}
			
			//LOg
			UltraDebug("-DTLSICETransport::onData() | Sending REMB [ssrc:%u,mid:'%s',count:%lu,bitrate:%u]\n",group->media.ssrc,group->mid.c_str(),ssrcs.size(),bitrate);
			
			// SSRC of media source (32 bits):  Always 0; this is the same convention as in [RFC5104] section 4.2.2.2 (TMMBN).
			auto remb = rtcp->CreatePacket<RTCPPayloadFeedback>(RTCPPayloadFeedback::ApplicationLayerFeeedbackMessage,group->media.ssrc,WORD(0));
			//Send estimation
			remb->AddField(RTCPPayloadFeedback::ApplicationLayerFeeedbackField::CreateReceiverEstimatedMaxBitrate(ssrcs,bitrate));
		}
		
		//If there is no outgoing stream, send NACK request on media sourcefor calculating RTT
		if (outgoing.empty() && group->media.ssrc == source->ssrc && group->rtx.ssrc)
		{
			//Get last seq num for calculating rtt based on rtx
			WORD last = group->SetRTTRTX(now);
			//If we have received at least one 
			if (last)
			{
				//We try to calculate rtt based on rtx
				auto nack = rtcp->CreatePacket<RTCPRTPFeedback>(RTCPRTPFeedback::NACK,mainSSRC,group->media.ssrc);
				//Request it
				nack->AddField(std::make_shared<RTCPRTPFeedback::NACKField>(last,0));
			}
		}
	
		//Send it
		Send(rtcp);
	}
	
	//Done
	return 1;
}

DWORD DTLSICETransport::SendProbe(const RTPPacket::shared& original)
{
	//Check packet
	if (!original)
		//Done
		return Error("-DTLSICETransport::SendProbe() | No packet\n");
	
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
		//Done
		return Warning("-DTLSICETransport::SendProbe() | Send SRTPSession is not setup yet\n");;
	
	//Get ssrc
	DWORD ssrc = original->GetSSRC();

	//Get outgoing group
	RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
	
	//If not found
	if (!group)
		//Error
		return Warning("-DTLSICETransport::SendProbe() | Outgoind source not registered for ssrc:%u\n",ssrc);

	//Get current time
	auto now = getTime();
	
	//Create resend packet
	auto packet = original->Clone();

	//Try to send it via rtx
	BYTE apt = sendMaps.apt.GetTypeForCodec(packet->GetPayloadType());
		
	//Check if we ar using rtx or not
	if (!group->rtx.ssrc || apt==RTPMap::NotFound)
		return Error("-DTLSICETransport::SendProbe() | No rtx or apt found [group:%p,ssrc:%u,apt:%d]\n", group, group->rtx.ssrc, apt);
	
	//Get rtx source
	RTPOutgoingSource& source = group->rtx;
	
	//Update RTX headers
	packet->SetSSRC(source.ssrc);
	packet->SetOSN(source.NextSeqNum());
	packet->SetPayloadType(apt);
	//No padding
	packet->SetPadding(0);
	
	//Add transport wide cc on video
	if (group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
		//Set transport wide seq num
		packet->SetTransportSeqNum(++transportSeqNum);
	else
		//Disable transport wide cc
		packet->DisableTransportSeqNum();
	
	//If we are using abs send time for sending
	if (sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::AbsoluteSendTime)!=RTPMap::NotFound)
		//Set abs send time
		packet->SetAbsSentTime(now/1000);
	else
		//Disable it
		packet->DisableAbsSentTime();
	
	//Disable rid & repair id
	packet->DisableRId();
	packet->DisableRepairedId();
	
	//Update mid
	if (!group->mid.empty())
		//Set new mid
		packet->SetMediaStreamId(group->mid);
	else
		//Disable it
		packet->DisableMediaStreamId();
	
	//No frame markings
	packet->DisableFrameMarkings();

	//Pick one packet buffer from the pool
	Packet buffer = packetPool.pick();
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize data
	int len = packet->Serialize(data,size,sendMaps.ext);
	
	//IF failed
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Log warning and exit
		return Warning("-DTLSICETransport::SendProbe() | Could not serialize packet\n");
	}

	//If we don't have an active candidate yet
	if (!active)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Log warning and exit
		//Error
		return Warning("-DTLSICETransport::SendProbe() | We don't have an active candidate yet\n");
	}

	//If dumping
	if (dumper && dumpOutRTP)
	{
		//Get truncate size
		DWORD truncate = dumpRTPHeadersOnly ? len - packet->GetMediaLength() + 16 : 0;
		//Write udp packet
		dumper->WriteUDP(now/1000,0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len,truncate);
	}

	//Encript
	len = send.ProtectRTP(data,len);
		
	//Check size
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Log warning and exit
		//Error
		return Error("-DTLSICETransport::SendProbe() | Error protecting RTP packet [ssrc:%u,%s]\n",source.ssrc,send.GetLastError());
	}
	
	//Store candidate before unlocking
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);

	//Check if we are using transport wide for this packet
	if (packet->HasTransportWideCC() && senderSideEstimationEnabled)
		//Send packet and update stats in callback
		sender->Send(candidate, std::move(buffer), [
				weak = std::weak_ptr<SendSideBandwidthEstimation>(senderSideBandwidthEstimator),
				stats = PacketStats::CreateProbing(packet, len, now)
			](std::chrono::milliseconds now)  mutable {
				//Get shared pointer from weak reference
				auto senderSideBandwidthEstimator = weak.lock();
				//If already gone
				if (!senderSideBandwidthEstimator)
					//Ignore
					return;
				//Update sent timestamp
				stats.timestamp = now.count();
				//Add new stat
				senderSideBandwidthEstimator->SentPacket(stats);
			}
		);
	else
		//Send packet
		sender->Send(candidate, std::move(buffer));
	
	//Update current time after sending
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
		
	//Update last send time and stats
	source.Update(now/1000, packet, len);
	
	//Log("-DTLSICETransport::SendProbe() |  Sent probe [size:%d]\n", len);

	return len;
}

DWORD DTLSICETransport::SendProbe(RTPOutgoingSourceGroup *group,BYTE padding)
{
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
		//Error
		return Warning("-DTLSICETransport::SendProbe() | Send SRTPSession is not setup\n");
	
	//Overrride headers
	RTPHeader		header;
	RTPHeaderExtension	extension;
	
	//Check if we ar using rtx or not
	bool rtx = group->rtx.ssrc && !sendMaps.apt.empty();
		
	//Check which source are we using
	RTPOutgoingSource& source = rtx ? group->rtx : group->media;
	
	//Get extended sequence number
	DWORD extSeqNum;

	//If it is using rtx (i.e. not firefox)
	if (rtx)
	{
		//Update RTX headers
		header.ssrc		= source.ssrc;
		header.payloadType	= sendMaps.apt.GetFirstCodecType();
		header.sequenceNumber	= extSeqNum = source.NextSeqNum();
		header.timestamp	= source.lastTimestamp++;
		//Padding
		header.padding		= 1;
	} else {
		//Update normal headers
		header.ssrc		= source.ssrc;
		header.payloadType	= source.lastPayloadType;
		header.sequenceNumber	= extSeqNum = source.AddGapSeqNum();
		header.timestamp	= source.lastTimestamp;
		//Padding
		header.padding		= 1;
	}
	
	//Get current time
	auto now = getTime();

	//Add transport wide cc on video
	if (group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
	{
		//Add extension
		header.extension = true;
		//Add transport
		extension.hasTransportWideCC = true;
		extension.transportSeqNum = ++transportSeqNum;
	}
	
	//If we are using abs send time for sending
	if (sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::AbsoluteSendTime)!=RTPMap::NotFound)
	{
		//Use extension
		header.extension = true;
		//Set abs send time
		extension.hasAbsSentTime = true;
		extension.absSentTime = now/1000;
	}
	
	//Pick one packet buffer from the pool
	Packet buffer = packetPool.pick();
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	int	len  = 0;

	//Serialize header
	int n = header.Serialize(data,size);

	//Comprobamos que quepan
	if (!n)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return Error("-DTLSICETransport::SendProbe() | Error serializing rtp headers\n");
	}

	//Inc len
	len += n;

	//If we have extension
	if (header.extension)
	{
		//Serialize
		n = extension.Serialize(sendMaps.ext,data+len,size-len);
		//Comprobamos que quepan
		if (!n)
		{
			//Return packet to pool
			packetPool.release(std::move(buffer));
			//Error
			return Error("-DTLSICETransport::SendProbe() | Error serializing rtp extension headers\n");
		}
		//Inc len
		len += n;
	}

	//Set 0 padding
	memset(data+len,0,padding);
	
	//Set pateckt length
	len += padding;
	
	//Set padding size in last byte of the padding
	data[len-1] = padding;

	//If we don't have an active candidate yet
	if (!active)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return Debug("-DTLSICETransport::SendProbe() | We don't have an active candidate yet\n");
	}

	//If dumping
	if (dumper && dumpOutRTP)
	{
		//Get truncate size
		DWORD truncate = dumpRTPHeadersOnly ? len - padding : 0;
		//Write udp packet
		dumper->WriteUDP(now/1000,0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len,truncate);
	}

	//Encript
	len = send.ProtectRTP(data,len);
		
	//Check size
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return Error("-RTPTransport::SendProbe() | Error protecting RTP packet [ssrc:%u,%s]\n",source.ssrc,send.GetLastError());
	}

	//Store candidate before unlocking
	ICERemoteCandidate* candidate = active;
	//Set buffer size
	buffer.SetSize(len);

	if(extension.hasTransportWideCC && senderSideEstimationEnabled)
		//Send packet and update stats in callback
		sender->Send(candidate, std::move(buffer),[
			weak = std::weak_ptr<SendSideBandwidthEstimation>(senderSideBandwidthEstimator),
			stats = PacketStats::CreateProbing(
				extension.transportSeqNum,
				header.ssrc,
				extSeqNum,
				len,
				0,
				header.timestamp,
				now,
				false
			)](std::chrono::milliseconds now) mutable {
				//Get shared pointer from weak reference
				auto senderSideBandwidthEstimator = weak.lock();
				//If already gone
				if (!senderSideBandwidthEstimator)
					//Ignore
					return;
				//Update sent timestamp
				stats.timestamp = now.count();
				//Add new stat
				senderSideBandwidthEstimator->SentPacket(stats);
			}
		);
	else
		//Send packet
		sender->Send(candidate,std::move(buffer));
	
	//Update now
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
	
	//Update last send time and stats
	source.Update(now/1000, header, len);

	return len;
}

void DTLSICETransport::ReSendPacket(RTPOutgoingSourceGroup *group,WORD seq)
{
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
		//Error
		return (void)Debug("-DTLSICETransport::ReSendPacket() | Send SRTPSession is not setup yet\n");
	
	//Get current time
	auto now = getTime();
	
	//Update rtx bitrate
	auto instant = rtxBitrate.Update(now/1000) * 8;

	//Log
	UltraDebug("-DTLSICETransport::ReSendPacket() | resending [seq:%d,ssrc:%u,rtx:%u,instant:%llu, isWindow:%d, count:%d, empty:%d]\n", seq, group->media.ssrc, group->rtx.ssrc, instant, rtxBitrate.IsInWindow(), rtxBitrate.GetCount(), rtxBitrate.IsEmpty());

	// If rtt is too large, disable RTX
	if (rtt > RtxRttThresholdMs)
	{
		return (void)UltraDebug("-DTLSICETransport::ReSendPacket() | Too large RTT, skiping rtx. RTT:%d\n", rtt);
	}

	//if sse is enabled
	if (senderSideEstimationEnabled && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC) != RTPMap::NotFound)
	{
		//Get target bitrate
		DWORD targetBitrate = senderSideBandwidthEstimator->GetTargetBitrate();
	
		//Check if we are sending way to much bitrate
		if (targetBitrate && rtxBitrate.GetInstantAvg()*8 > targetBitrate * MaxRTXOverhead)
			//Error
			return (void)UltraDebug("-DTLSICETransport::ReSendPacket() | Too much bitrate on rtx, skiping rtx:%lld estimated:%u target:%d\n",(uint64_t)(rtxBitrate.GetInstantAvg()*8), senderSideBandwidthEstimator->GetEstimatedBitrate(), targetBitrate);
		
		if (targetBitrate && outgoingBitrate.GetInstantAvg()*8 > targetBitrate)
			//Error
			return (void)UltraDebug("-DTLSICETransport::ReSendPacket() | Too much outgoing bitrate, skiping rtx. outgoing:%lld estimated:%u target:%d\n",(uint64_t)(outgoingBitrate.GetInstantAvg()*8), senderSideBandwidthEstimator->GetEstimatedBitrate(), targetBitrate);
	}

	//Check if we can retransmit the packet, or we have rtx recently
	if (!group->isRTXAllowed(seq, now/1000))
		//Debug
		return (void)UltraDebug("-DTLSICETransport::ReSendPacket() | rtx not allowed for packet [seq:%d,ssrc:%u,rtx:%u]\n", seq, group->media.ssrc, group->rtx.ssrc);
	
	//Find packet to retransmit
	auto original = group->GetPacket(seq);

	//If we don't have it anymore
	if (!original)
		//Debug
		return (void)UltraDebug("-DTLSICETransport::ReSendPacket() | packet not found[seq:%d,ssrc:&%u,rtx:%u]\n",seq,group->media.ssrc,group->rtx.ssrc);
	
	//Create resend packet
	auto packet = original->Clone();
	
	//Try to send it via rtx
	BYTE apt = sendMaps.apt.GetTypeForCodec(packet->GetPayloadType());
		
	//Check if we ar using rtx or not
	bool rtx = group->rtx.ssrc && apt!=RTPMap::NotFound;
		
	//Check which source are we using
	RTPOutgoingSource& source = rtx ? group->rtx : group->media;
	
	//If it is using rtx (i.e. not firefox)
	if (rtx)
	{
		//Update RTX headers
		packet->SetSSRC(source.ssrc);
		packet->SetOSN(source.NextSeqNum());
		packet->SetPayloadType(apt);
		//No padding
		packet->SetPadding(0);
	}
	
	//Add transport wide cc on video
	if (group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
		//Set transport wide seq num
		packet->SetTransportSeqNum(++transportSeqNum);
	else
		//Disable transport wide cc
		packet->DisableTransportSeqNum();
	
	//If we are using abs send time for sending
	if (sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::AbsoluteSendTime)!=RTPMap::NotFound)
		//Set abs send time
		packet->SetAbsSentTime(now/1000);
	else
		//Disable it
		packet->DisableAbsSentTime();
	
	//Disable rid & repair id
	packet->DisableRId();
	packet->DisableRepairedId();
	
	//Update mid
	if (!group->mid.empty())
		//Set new mid
		packet->SetMediaStreamId(group->mid);
	else
		//Disable it
		packet->DisableMediaStreamId();
	
	//No frame markings
	packet->DisableFrameMarkings();
	
	//Pick one packet buffer from the pool
	Packet buffer = packetPool.pick();
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize data
	int len = packet->Serialize(data,size,sendMaps.ext);
	
	//IF failed
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Log warnign and exit
		return (void)Warning("-DTLSICETransport::ReSendPacket() | Could not serialize packet\n");
	}

	//If we don't have an active candidate yet
	if (!active)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return (void)Warning("-DTLSICETransport::ReSendPacket() | We don't have an active candidate yet\n");
	}

	//If dumping
	if (dumper && dumpOutRTP)
	{
		//Get truncate size
		DWORD truncate = dumpRTPHeadersOnly ? len - packet->GetMediaLength() + 16 : 0;
		//Write udp packet
		dumper->WriteUDP(now/1000,0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len,truncate);
	}
		
	//Encript
	len = send.ProtectRTP(data,len);
		
	//Check size
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return (void)Error("-RTPTransport::ReSendPacket() | Error protecting RTP packet [ssrc:%u,%s]\n",source.ssrc,send.GetLastError());
	}
	
	//Store candidate before unlocking
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);

	//Check if we are using transport wide for this packet
	if (packet->HasTransportWideCC() && senderSideEstimationEnabled)
		//Send packet and update stats in callback
		sender->Send(candidate, std::move(buffer), [
			weak = std::weak_ptr<SendSideBandwidthEstimation>(senderSideBandwidthEstimator),
				stats = PacketStats::CreateRTX(packet, len, now)
				](std::chrono::milliseconds now) mutable {
				//Get shared pointer from weak reference
				auto senderSideBandwidthEstimator = weak.lock();
				//If already gone
				if (!senderSideBandwidthEstimator)
					//Ignore
					return;
				//Update sent timestamp
				stats.timestamp = now.count();
				//Add new stat
				senderSideBandwidthEstimator->SentPacket(stats);
			}
		);
	else
		//Send packet
		sender->Send(candidate, std::move(buffer));

	
	//Update current time after sending
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
	rtxBitrate.Update(now/1000,len);
	
	//Update stats
	source.Update(now/1000, packet, len);

	//Update rtx time
	group->SetRTXTime(seq, now/1000);
}

void  DTLSICETransport::ActivateRemoteCandidate(ICERemoteCandidate* candidate,bool useCandidate, DWORD priority)
{
	//Debug
	//UltraDebug("-DTLSICETransport::ActivateRemoteCandidate() | Remote candidate [%s:%hu,use:%d,prio:%d,active:%p]\n",candidate->GetIP(),candidate->GetPort(),useCandidate,priority,active);
	
	//Restart timer
	iceTimeoutTimer->Again(IceTimeout);
	
	//Should we set this candidate as the active one
	if (!active || (useCandidate && candidate!=active))
	{
		//Debug
		Debug("-DTLSICETransport::ActivateRemoteCandidate() | Activating candidate [%s:%hu,use:%d,prio:%d,dtls:%d]\n",candidate->GetIP(),candidate->GetPort(),useCandidate,priority,dtls.GetSetup());	
		
		//Send data to this one from now on
		active = candidate;
		
		//Launch event
		if (listener)listener->onRemoteICECandidateActivated(candidate->GetIP(),candidate->GetPort(),priority);
	}
	
	// Needed for DTLS in client mode (otherwise the DTLS "Client Hello" is not sent over the wire)
	if (dtls.GetSetup()!=DTLSConnection::SETUP_PASSIVE) 
		//Trigger manually
		onDTLSPendingData();
}

void DTLSICETransport::SetLocalProperties(const Properties& properties)
{
	std::vector<Properties> codecs;
	std::vector<Properties> extensions;
	
	//For each property
	for (Properties::const_iterator it=properties.begin();it!=properties.end();++it)
		Debug("-DTLSICETransport::SetLocalProperties | Setting property [%s:%s]\n",it->first.c_str(),it->second.c_str());
	
	//Cleant maps
	sendMaps.rtp.clear();
	sendMaps.ext.clear();
	sendMaps.apt.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("audio.codecs",codecs);

	//For each codec
	for (auto it = codecs.begin(); it!=codecs.end(); ++it)
	{
		//Get codec by name
		AudioCodec::Type codec = AudioCodec::GetCodecForName(it->GetProperty("codec"));
		//Get codec type
		BYTE type = it->GetProperty("pt",0);
		//Check
		if (type && codec != AudioCodec::UNKNOWN)
		{
			//ADD it
			sendMaps.rtp.SetCodecForType(type, codec);
			//Get rtx
			BYTE rtx = it->GetProperty("rtx", 0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				sendMaps.rtp.SetCodecForType(rtx, AudioCodec::RTX);
				//Add the reverse one
				sendMaps.apt.SetCodecForType(rtx, type);
			}
		}
	}
	
	//Clear codecs
	codecs.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("audio.ext",extensions);
	
	//For each codec
	for (auto it = extensions.begin(); it!=extensions.end(); ++it)
	{
		//Get Extension for name
		RTPHeaderExtension::Type ext = RTPHeaderExtension::GetExtensionForName(it->GetProperty("uri"));
		//Get extension id
		BYTE id = it->GetProperty("id",0);
		//ADD it
		sendMaps.ext.SetCodecForType(id, ext);
	}
	
	//Clear extension
	extensions.clear();
	
	//Get video codecs
	properties.GetChildrenArray("video.codecs",codecs);
	
	//For each codec
	for (auto it = codecs.begin(); it!=codecs.end(); ++it)
	{
		//Get codec by name
		VideoCodec::Type codec = VideoCodec::GetCodecForName(it->GetProperty("codec"));
		//Get codec type
		BYTE type = it->GetProperty("pt",0);
		//Check
		if (type && codec!=VideoCodec::UNKNOWN)
		{
			//ADD it
			sendMaps.rtp.SetCodecForType(type, codec);
			//Get rtx
			BYTE rtx = it->GetProperty("rtx",0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				sendMaps.rtp.SetCodecForType(rtx, VideoCodec::RTX);
				//Add the reverse one
				sendMaps.apt.SetCodecForType(rtx, type);
			}
		}
	}
	
	//Clear codecs
	codecs.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("video.ext",extensions);
	
	//For each codec
	for (auto it = extensions.begin(); it!=extensions.end(); ++it)
	{
		//Get Extension for name
		RTPHeaderExtension::Type ext = RTPHeaderExtension::GetExtensionForName(it->GetProperty("uri"));
		//Get extension id
		BYTE id = it->GetProperty("id",0);
		//ADD it
		sendMaps.ext.SetCodecForType(id, ext);
	}
	
	//Clear extension
	extensions.clear();

}

void DTLSICETransport::SetSRTPProtectionProfiles(const std::string& profiles)
{
	dtls.SetSRTPProtectionProfiles(profiles);
}

void DTLSICETransport::SetRemoteProperties(const Properties& properties)
{
	//For each property
	for (Properties::const_iterator it=properties.begin();it!=properties.end();++it)
		//Log
		Debug("-DTLSICETransport::SetRemoteProperties | Setting property [%s:%s]\n",it->first.c_str(),it->second.c_str());
	
	std::vector<Properties> codecs;
	std::vector<Properties> extensions;
	
	//Cleant maps
	recvMaps.rtp.clear();
	recvMaps.ext.clear();
	recvMaps.apt.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("audio.codecs",codecs);

	//For each codec
	for (auto it = codecs.begin(); it!=codecs.end(); ++it)
	{
		//Get codec by name
		AudioCodec::Type codec = AudioCodec::GetCodecForName(it->GetProperty("codec"));
		//Get codec type
		BYTE type = it->GetProperty("pt",0);
		//Check
		if (type && codec != AudioCodec::UNKNOWN)
		{
			//ADD it
			recvMaps.rtp.SetCodecForType(type, codec);
			//Get rtx
			BYTE rtx = it->GetProperty("rtx", 0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				recvMaps.rtp.SetCodecForType(rtx, AudioCodec::RTX);
				//Add the reverse one
				recvMaps.apt.SetCodecForType(rtx, type);
			}
		}
	}
	
	//Clear codecs
	codecs.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("audio.ext",extensions);
	
	//For each codec
	for (auto it = extensions.begin(); it!=extensions.end(); ++it)
	{
		//Get Extension for name
		RTPHeaderExtension::Type ext = RTPHeaderExtension::GetExtensionForName(it->GetProperty("uri"));
		//Get extension id
		BYTE id = it->GetProperty("id",0);
		//ADD it
		recvMaps.ext.SetCodecForType(id, ext);
	}
	
	//Clear extension
	extensions.clear();
	
	//Get video codecs
	properties.GetChildrenArray("video.codecs",codecs);
	
	//For each codec
	for (auto it = codecs.begin(); it!=codecs.end(); ++it)
	{
		//Get codec by name
		VideoCodec::Type codec = VideoCodec::GetCodecForName(it->GetProperty("codec"));
		//Get codec type
		BYTE type = it->GetProperty("pt",0);
		//Check
		if (type && codec!=VideoCodec::UNKNOWN)
		{
			//ADD it
			recvMaps.rtp.SetCodecForType(type, codec);
			//Get rtx
			BYTE rtx = it->GetProperty("rtx",0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				recvMaps.rtp.SetCodecForType(rtx, VideoCodec::RTX);
				//Add the reverse one
				recvMaps.apt.SetCodecForType(rtx, type);
			}
		}
	}
	
	//Clear codecs
	codecs.clear();
	
	//Get audio codecs
	properties.GetChildrenArray("video.ext",extensions);
	
	//For each codec
	for (auto it = extensions.begin(); it!=extensions.end(); ++it)
	{
		//Get Extension for name
		RTPHeaderExtension::Type ext = RTPHeaderExtension::GetExtensionForName(it->GetProperty("uri"));
		//Get extension id
		BYTE id = it->GetProperty("id",0);
		//ADD it
		recvMaps.ext.SetCodecForType(id, ext);
	}
	
	//Clear extension
	extensions.clear();
}
int DTLSICETransport::Dump(UDPDumper* dumper, bool inbound, bool outbound, bool rtcp, bool rtpHeadersOnly)
{
	Debug("-DTLSICETransport::Dump() | [inbound:%d,outboud:%d,rtcp:%d,rtpHeadersOnly:%d]\n",inbound,outbound,rtcp,rtpHeadersOnly);
	
	//Done
	int done = 1;
	//Execute on timer thread
	timeService.Sync([&](auto now){
		//Check we are not dumping
		if (this->dumper)
		{
			//Error
			done = Error("-DTLSICETransport::Dump() | Already dumping\n");
			return;
		}

		//Store pcap as dumper
		this->dumper.reset(dumper);

		//What to dumo
		dumpInRTP		= inbound;
		dumpOutRTP		= outbound;
		dumpRTCP		= rtcp;
		dumpRTPHeadersOnly	= rtpHeadersOnly;
	});
	//Done
	return done;
}

int DTLSICETransport::StopDump()
{
	Debug("-DTLSICETransport::StopDump()\n");
	
	//Done
	int done = 1;
	//Execute on timer thread
	timeService.Sync([&](auto now){
		//Check we are not dumping
		if (!this->dumper)
		{
			//Error
			done = Error("-DTLSICETransport::StopDump() | Not dumping\n");
			return;
		}
		//Close dumper
		this->dumper->Close();
		//Not dumping
		this->dumper.reset();
	});
	//Done
	return done;
}

int DTLSICETransport::Dump(const char* filename, bool inbound, bool outbound, bool rtcp, bool rtpHeadersOnly)
{
	Log("-DTLSICETransport::Dump() | [pcap:%s,inbound:%d,outboud:%d,rtcp:%d,rtpHeadersOnly:%d]\n",filename,inbound,outbound,rtcp,rtpHeadersOnly);
	
	//Done
	int done = 1;
	//Execute on timer thread
	timeService.Sync([&](auto now){
		//Check we are not dumping
		if (this->dumper)
		{
			//Error
			done = Error("Already dumping\n");
			return;
		}

		//Create dumper file
		PCAPFile* pcap = new PCAPFile();

		//Open it
		if (!pcap->Open(filename))
		{
			//Error
			done = Error("Error opening pcap file for dumping\n");
			//Error
			delete(pcap);
			return ;
		}
		//Store pcap as dumper
		this->dumper.reset(pcap);

		//What to dump
		dumpInRTP		= inbound;
		dumpOutRTP		= outbound;
		dumpRTCP		= rtcp;
		dumpRTPHeadersOnly	= rtpHeadersOnly;
	});
	
	//Done
	return done;
}

int DTLSICETransport::DumpBWEStats(const char* filename)
{
	return senderSideBandwidthEstimator->Dump(filename);
}

int DTLSICETransport::StopDumpBWEStats()
{
	return senderSideBandwidthEstimator->StopDump();
}


void DTLSICETransport::Reset()
{
	Log("-DTLSICETransport::Reset()\n");

	//Execute on timer thread
	timeService.Sync([=](auto now){
		
		//Reset srtp
		send.Reset();
		recv.Reset();

		//Check if dumping
		if (dumper)
		{
			//Close dumper
			dumper->Close();
			//Delete it
			dumper.reset();
		}
		//No ice
		iceLocalUsername.clear();
		iceLocalPwd.clear();
		iceRemoteUsername.clear();
		iceRemotePwd.clear();
		dumper = NULL;
		dumpInRTP = false;
		dumpOutRTP = false;
		dumpRTCP = false;
	});
}

int DTLSICETransport::SetLocalCryptoSDES(const char* suite,const BYTE* key,const DWORD len)
{
	Log("-DTLSICETransport::SetLocalCryptoSDES() | [suite:%s]\n",suite);
	
	//Set sending srtp session
	if (!send.Setup(suite,key,len))
		//Error
		return Error("-DTLSICETransport::SetLocalCryptoSDES() | Error [%s]\n",send.GetLastError());
	//Done
	return 1;
}


int DTLSICETransport::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	Log("-DTLSICETransport::SetLocalSTUNCredentials() | [frag:%s,pwd:%s]\n",username,pwd);
	//Store values
	iceLocalUsername = username;
	iceLocalPwd = pwd;
	//Ok
	return 1;
}


int DTLSICETransport::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	Log("-DTLSICETransport::SetRemoteSTUNCredentials() |  [frag:%s,pwd:%s]\n",username,pwd);
	
	//Store values
	iceRemoteUsername = username;
	iceRemotePwd = pwd;
	//Ok
	return 1;
}

int DTLSICETransport::SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint)
{
	Log("-DTLSICETransport::SetRemoteCryptoDTLS | [setup:%s,hash:%s,fingerprint:%s]\n",setup,hash,fingerprint);

	//Set Suite
	if (strcasecmp(setup,"active")==0)
		dtls.SetRemoteSetup(DTLSConnection::SETUP_ACTIVE);
	else if (strcasecmp(setup,"passive")==0)
		dtls.SetRemoteSetup(DTLSConnection::SETUP_PASSIVE);
	else if (strcasecmp(setup,"actpass")==0)
		dtls.SetRemoteSetup(DTLSConnection::SETUP_ACTPASS);
	else if (strcasecmp(setup,"holdconn")==0)
		dtls.SetRemoteSetup(DTLSConnection::SETUP_HOLDCONN);
	else
		return Error("-DTLSICETransport::SetRemoteCryptoDTLS | Unknown setup");

	//Set fingerprint
	if (strcasecmp(hash,"SHA-1")==0)
		dtls.SetRemoteFingerprint(DTLSConnection::SHA1,fingerprint);
	else if (strcasecmp(hash,"SHA-224")==0)
		dtls.SetRemoteFingerprint(DTLSConnection::SHA224,fingerprint);
	else if (strcasecmp(hash,"SHA-256")==0)
		dtls.SetRemoteFingerprint(DTLSConnection::SHA256,fingerprint);
	else if (strcasecmp(hash,"SHA-384")==0)
		dtls.SetRemoteFingerprint(DTLSConnection::SHA384,fingerprint);
	else if (strcasecmp(hash,"SHA-512")==0)
		dtls.SetRemoteFingerprint(DTLSConnection::SHA512,fingerprint);
	else
		return Error("-DTLSICETransport::SetRemoteCryptoDTLS | Unknown hash");

	//Starting
	SetState(DTLSState::Connecting);
	//Ok
	return 1;
}

int DTLSICETransport::SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len)
{
	Log("-DTLSICETransport::SetRemoteCryptoSDES() | [suite:%s]\n",suite);
	
	//Set sending srtp session
	if (!recv.Setup(suite,key,len))
		//Error
		return Error("-DTLSICETransport::SetRemoteCryptoSDES() | Error [%s]\n",send.GetLastError());
	//Done
	return 1;
}

void DTLSICETransport::onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize)
{
	Log("-DTLSICETransport::onDTLSSetup() [suite:%d]\n",suite);

	switch (suite)
	{
		case DTLSConnection::AES_CM_128_HMAC_SHA1_80:
			//Set keys
			SetLocalCryptoSDES("AES_CM_128_HMAC_SHA1_80",localMasterKey,localMasterKeySize);
			SetRemoteCryptoSDES("AES_CM_128_HMAC_SHA1_80",remoteMasterKey,remoteMasterKeySize);
			break;
		case DTLSConnection::AES_CM_128_HMAC_SHA1_32:
			//Set keys
			SetLocalCryptoSDES("AES_CM_128_HMAC_SHA1_32",localMasterKey,localMasterKeySize);
			SetRemoteCryptoSDES("AES_CM_128_HMAC_SHA1_32",remoteMasterKey,remoteMasterKeySize);
			break;
		case DTLSConnection::AEAD_AES_128_GCM:
			//Set keys
			SetLocalCryptoSDES("AEAD_AES_128_GCM",localMasterKey,localMasterKeySize);
			SetRemoteCryptoSDES("AEAD_AES_128_GCM",remoteMasterKey,remoteMasterKeySize);
			break;
		case DTLSConnection::AEAD_AES_256_GCM:
			//Set keys
			SetLocalCryptoSDES("AEAD_AES_256_GCM",localMasterKey,localMasterKeySize);
			SetRemoteCryptoSDES("AEAD_AES_256_GCM",remoteMasterKey,remoteMasterKeySize);
			break;
		default:
			//Error
			SetState(DTLSState::Failed);
			//Log
			Error("-DTLSICETransport::onDTLSSetup() | Unknown suite\n");
			//Exit
			return;
	}
	
	//Done
	SetState(DTLSState::Connected);
	
	//Check if we need to start or stop the timer
	CheckProbeTimer();
}

void DTLSICETransport::onDTLSSetupError()
{
	//Set new state
	SetState(DTLSState::Failed);
}

void DTLSICETransport::onDTLSShutdown()
{
	//Set new state
	SetState(DTLSState::Closed);
}

bool DTLSICETransport::AddOutgoingSourceGroup(const RTPOutgoingSourceGroup::shared& group)
{
	//It must contain media ssrc
	if (!group)
		return Error("-DTLSICETransport::AddOutgoingSourceGroup() | Empty group\n");

	//Log
	Log("-DTLSICETransport::AddOutgoingSourceGroup() [group:%p,ssrc:%u,fec:%u,rtx:%u]\n",group.get(), group->media.ssrc, group->fec.ssrc, group->rtx.ssrc);
	
	//Dispatch to the event loop thread
	timeService.Async([=](auto now){

		//Get ssrcs
		const auto media = group->media.ssrc;
		const auto fec = group->fec.ssrc;
		const auto rtx = group->rtx.ssrc;

		//TODO: pass a callback for confirming creation
		//Check they are not already assigned
		if (media && outgoing.find(media) != outgoing.end())
		{
			//Error
			Error("-DTLSICETransport::AddOutgoingSourceGroup() | media ssrc already assigned");
			return;
		}

		if (fec && outgoing.find(fec)!=outgoing.end())
		{
			//Error
			Error("-DTLSICETransport::AddOutgoingSourceGroup() | fec ssrc already assigned");
			return;
		}

		if (rtx && outgoing.find(rtx) != outgoing.end())
		{
			//Error
			Error("-DTLSICETransport::AddOutgoingSourceGroup() | rtx ssrc already assigned");
			return;
		}
	
		//Add it for each group ssrc
		if (media)
		{
			outgoing[media] = group.get();
			send.AddStream(media);
		}
		if (fec)
		{
			outgoing[fec] = group.get();
			send.AddStream(fec);
		}
		if (rtx)
		{
			outgoing[rtx] = group.get();
			send.AddStream(rtx);
		}

		//If we don't have a mainSSRC
		if (mainSSRC==1 && media)
			//Set it
			mainSSRC = media;

		//Check if we need to start or stop the timer
		CheckProbeTimer();

		//TODO: Send SDES
	});
	
	//Done
	return true;
	
}

bool DTLSICETransport::RemoveOutgoingSourceGroup(const RTPOutgoingSourceGroup::shared&  group)
{
	Log("-DTLSICETransport::RemoveOutgoingSourceGroup() [ssrc:%u,fec:%u,rtx:%u]\n", group->media.ssrc, group->fec.ssrc, group->rtx.ssrc);

	//Dispatch to the event loop thread
	timeService.Async([=](auto now){
		Log("-DTLSICETransport::RemoveOutgoingSourceGroup() | Async [ssrc:%u,fec:%u,rtx:%u]\n", group->media.ssrc, group->fec.ssrc, group->rtx.ssrc);
		//Get ssrcs
		std::vector<DWORD> ssrcs;
		const auto media = group->media.ssrc;
		const auto fec = group->fec.ssrc;
		const auto rtx   = group->rtx.ssrc;
		
		//If got media ssrc
		if (media)
		{
			//Remove from ssrc mapping and srtp session
			outgoing.erase(media);
			send.RemoveStream(media);
			//Add group ssrcs
			ssrcs.push_back(media);
		}
		//IF got fec ssrc
		if (fec)
		{
			//Remove from ssrc mapping and srtp session
			outgoing.erase(fec);
			send.RemoveStream(fec);
			//Add group ssrcs
			ssrcs.push_back(fec);
		}
		//IF got rtx ssrc
		if (rtx)
		{
			//Remove from ssrc mapping and srtp session
			outgoing.erase(rtx);
			send.RemoveStream(rtx);
			//Add group ssrcs
			ssrcs.push_back(rtx);
			//Clear history
			//TODO: make it fine grained
			history.clear();
		}
		
		//If it was our main ssrc
		if (mainSSRC==group->media.ssrc)
			//Set first
			mainSSRC = outgoing.begin()!=outgoing.end() ? outgoing.begin()->second->media.ssrc : 1;
		
		//Send BYE
		Send(RTCPCompoundPacket::Create(RTCPBye::Create(ssrcs,"terminated")));

		//If last one
		if (outgoing.size()==0 && probingTimer)
			//Stop probing timer
			probingTimer->Cancel();
	});
	
	//Done
	return true;
}

bool DTLSICETransport::AddIncomingSourceGroup(const RTPIncomingSourceGroup::shared& group)
{
	//It must contain media ssrc
	if (!group)
		return Error("-DTLSICETransport::AddIncomingSourceGroup() | Empty group\n");

	//RTX should only be enabled if RTX codec has been negotiated for the media type
	bool isRTXEnabled = group->type==MediaFrame::Video ? sendMaps.rtp.HasCodec(VideoCodec::RTX) : sendMaps.rtp.HasCodec(AudioCodec::RTX);

	//Log
	Log("-DTLSICETransport::AddIncomingSourceGroup() [mid:'%s',rid:'%s',ssrc:%u,rtx:%u,isRTXEnabled:%d,disableREMB:%d]\n",group->mid.c_str(),group->rid.c_str(),group->media.ssrc,group->rtx.ssrc,isRTXEnabled, disableREMB);
	
	//It must contain media ssrc
	if (!group->media.ssrc && group->rid.empty())
		return Error("No media ssrc or rid defined, stream will not be added\n");
	
	//Dispatch to the event loop thread
	timeService.Async([=](auto now){
		//Get ssrcs
		const auto media = group->media.ssrc;
		const auto rtx   = group->rtx.ssrc;
		
		//Check they are not already assigned
		if (media && incoming.find(media)!=incoming.end())
		{
			//Error
			Warning("-DTLSICETransport::AddIncomingSourceGroup() media ssrc already assigned\n");
			return;
		}
		
			
		if (rtx && incoming.find(rtx)!=incoming.end())
		{
			//Error
			Warning("-DTLSICETransport::AddIncomingSourceGroup() rtx ssrc already assigned\n");
			return;
		}

		//Add rid if any
		if (!group->rid.empty())
			rids[group->mid + "@" + group->rid] = group.get();

		//Add mid if any
		if (!group->mid.empty())
		{
			//Find mid 
			auto it = mids.find(group->mid);
			//If not there
			if (it!=mids.end())
				//Append
				it->second.insert(group.get());
			else
				//Add new set
				mids[group->mid] = { group.get() };
		}

		//Add it for each group ssrc
		if (media)
		{
			incoming[media] = group.get();
			recv.AddStream(media);
		}
		if (rtx)
		{
			incoming[rtx] = group.get();
			recv.AddStream(rtx);
		}

		//Set RTX supported flag only for video
		group->SetRTXEnabled(isRTXEnabled);

		//If it is video and the transport wide cc is not enabled enable and not overriding the bitrate estimation
		bool remb = group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC) == RTPMap::NotFound && !overrideBWE && !disableREMB;

		//Start distpaching
		group->Start(remb);
	});

	
	//Done
	return true;
}

bool DTLSICETransport::RemoveIncomingSourceGroup(const RTPIncomingSourceGroup::shared &group)
{
	Log("-DTLSICETransport::RemoveIncomingSourceGroup() [mid:'%s',rid:'%s',ssrc:%u,rtx:%u]\n",group->mid.c_str(),group->rid.c_str(),group->media.ssrc,group->rtx.ssrc);
	
	//Dispatch to the event loop thread
	timeService.Async([=](auto now){

		//Remove rid if any
		if (!group->rid.empty())
			rids.erase(group->mid + "@" + group->rid);

		//Find mid 
		auto it = mids.find(group->mid);
		//If found
		if (it!=mids.end())
		{
			//Erase group
			it->second.erase(group.get());
			//If it is empty now
			if(it->second.empty())
				//Remove from mids
				mids.erase(it);
		}

		//Get ssrcs
		const auto media = group->media.ssrc;
		const auto rtx   = group->rtx.ssrc;

		//If got media ssrc
		if (media)
		{
			//Remove from ssrc mapping and srtp session
			incoming.erase(media);
			recv.RemoveStream(media);
		}
		//IF got rtx ssrc
		if (rtx)
		{
			//Remove from ssrc mapping and srtp session
			incoming.erase(rtx);
			recv.RemoveStream(rtx);
		}

		//Stop distpaching
		group->Stop();
	});

	//Done
	return true;
}

int DTLSICETransport::Send(const RTCPCompoundPacket::shared &rtcp)
{

	TRACE_EVENT("rtp","DTLSICETransport::Send RTCP");

	//TODO: Assert event loop thread
	
	//Double check message
	if (!rtcp)
		//Log
		return Error("-DTLSICETransport::Send() | NULL rtcp message\n");
	
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
		//Log 
		return Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
	//Pick one packet buffer from the pool
	Packet buffer = packetPool.pick();
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize
	DWORD len = rtcp->Serialize(data,size);
	
	//Check result
	if (len<=0 || len>size)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Dump it
		rtcp->Dump();
		//Error
		return Error("-DTLSICETransport::Send() | Error serializing RTCP packet [len:%d,size:%d]\n",len,size);
	}
	
	//If we don't have an active candidate yet
	if (!active)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Log
		return Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");
	}

	//Get current time
	QWORD now = getTime();

	//If dumping
	if (dumper && dumpRTCP)
		//Write udp packet
		dumper->WriteUDP(now/1000,0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);

	//Encript
	len = send.ProtectRTCP(data,len);
	
	//Check error
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return Error("-DTLSICETransport::Send() | Error protecting RTCP packet [%s]\n",send.GetLastError());
	}

	//Store active candidate1889
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);
	//No error yet, send packet
	len = sender->Send(candidate,std::move(buffer));
	
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
	
	//Return length
	return len;
}

int DTLSICETransport::SendPLI(DWORD ssrc)
{
	//Log
	Debug("-DTLSICETransport::SendPLI() | [ssrc:%u]\n",ssrc);
	
	//Execute on the event loop thread and do not wait
	timeService.Async([=](auto now){
		//Get group
		RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);

		//If not found
		if (!group)
			return (void)Debug("-DTLSICETransport::SendPLI() | no incoming source found for [ssrc:%u]\n",ssrc);
		//Get now in ms
		auto ms = now.count();
		//Check if we have sent a PLI recently (less than half a second ago)
		if ((ms - group->media.lastPLI)<1E3/2)
			//We refuse to end more
			return (void)UltraDebug("-DTLSICETransport::SendPLI() | ignored, we send a PLI recently\n");
		//Update last PLI requested time
		group->media.lastPLI = ms;
		//And number of requested plis
		group->media.totalPLIs++;

		//Create rtcp sender retpor
		auto rtcp = RTCPCompoundPacket::Create();

		//Add to rtcp
		rtcp->CreatePacket<RTCPPayloadFeedback>(RTCPPayloadFeedback::PictureLossIndication,mainSSRC,ssrc);

		//Send packet
		Send(rtcp);
	});
	
	return 1;
}

int DTLSICETransport::Reset(DWORD ssrc)
{
	//Log
	Debug("-DTLSICETransport::Reset() | [ssrc:%u]\n", ssrc);

	//Execute on the event loop thread and do not wait
	timeService.Async([=](auto now) {
		//Get group
		RTPIncomingSourceGroup* group = GetIncomingSourceGroup(ssrc);

		//If not found
		if (!group)
			return (void)Debug("-DTLSICETransport::Reset() | no incoming source found for [ssrc:%u]\n", ssrc);

		//Reset buffers
		group->ResetPackets();

		//Reset srtp session
		recv.RemoveStream(group->media.ssrc);
		recv.AddStream(group->media.ssrc);

		//Reset
		group->media.Reset();

		//Check if there was an rtx ssrc
		if (group->rtx.ssrc)
		{
			//Reset srtp session
			recv.RemoveStream(group->rtx.ssrc);
			recv.AddStream(group->rtx.ssrc);
		}

		//Reset
		group->rtx.Reset();
	});

	return 1;
}

int DTLSICETransport::Send(const RTPPacket::shared& packet)
{
	//Check packet
	if (!packet)
		//Error
		return Error("-DTLSICETransport::Send() | Error null packet\n");
	
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
		//Error
		return Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");

	//Trace
	TRACE_EVENT("rtp", "DTLSICETransport::Send RTP",
		"ssrc", packet->GetSSRC(),
		"seqNum", packet->GetSeqNum());
	
	//Get ssrc
	DWORD ssrc = packet->GetSSRC();
	
	//Get outgoing group
	RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
	
	//If not found
	if (!group)
		//Error
		return Warning("-DTLSICETransport::Send() | Outgoind source not registered for ssrc:%u\n",packet->GetSSRC());
	
	//Get outgoing source
	RTPOutgoingSource& source = group->media;
	
        //Update headers
        packet->SetExtSeqNum(source.CorrectExtSeqNum(packet->GetExtSeqNum()));
        packet->SetSSRC(source.ssrc);
        
	packet->SetPayloadType(sendMaps.rtp.GetTypeForCodec(packet->GetCodec()));
	//No padding
	packet->SetPadding(0);

	//Add transport wide cc on video
	if (group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
		//Set transport wide seq num
		packet->SetTransportSeqNum(++transportSeqNum);
	else
		//Disable transport wide cc
		packet->DisableTransportSeqNum();

	//Get time
	auto now = getTime();
	
	//If we are using abs send time for sending
	if (sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::AbsoluteSendTime)!=RTPMap::NotFound)
		//Set abs send time
		packet->SetAbsSentTime(now/1000);
	else
		//Disable it
		packet->DisableAbsSentTime();
	
	//Disable rid & repair id
	packet->DisableRId();
	packet->DisableRepairedId();
	
	//Update mid
	if (!group->mid.empty())
		//Set new mid
		packet->SetMediaStreamId(group->mid);
	else
		//Disable it
		packet->DisableMediaStreamId();
	
	//No frame markings
	packet->DisableFrameMarkings();

	//If we are forcing video playout delay
	if (group->type == MediaFrame::Video && group->HasForcedPlayoutDelay() && packet->IsKeyFrame())
		//Set delay
		packet->SetPlayoutDelay(group->GetForcedPlayoutDelay());
	else
		//Disable playout delay
		packet->DisablePlayoutDelay();

	//if (group->type==MediaFrame::Video) UltraDebug("-DTLSICETransport::Send() | Sending RTP on media:%s sssrc:%u seq:%u pt:%u ts:%lu codec:%s\n",MediaFrame::TypeToString(group->type),source.ssrc,packet->GetSeqNum(),packet->GetPayloadType(),packet->GetTimestamp(),GetNameForCodec(group->type,packet->GetCodec()));
	
	//Pick one packet buffer from the pool
	Packet buffer = packetPool.pick();
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize data
	int len = packet->Serialize(data,size,sendMaps.ext);
	
	//IF failed
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Log warning and exit
		return Warning("-DTLSICETransport::Send() | Could not serialize packet\n");
	}

	//Add packet for RTX
	group->AddPacket(packet);
	
	//If we don't have an active candidate yet
	if (!active)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");
	}

	//If dumping
	if (dumper && dumpOutRTP)
	{
		//Get truncate size
		DWORD truncate = dumpRTPHeadersOnly ? len - packet->GetMediaLength() + 16 : 0;
		//Write udp packet
		dumper->WriteUDP(now/1000,0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len,truncate);
	}

	//Encript
	len = send.ProtectRTP(data,len);
	
	//Check error
	if (!len)
	{
		//Return packet to pool
		packetPool.release(std::move(buffer));
		//Error
		return Error("-RTPTransport::Send() | Error protecting RTP packet [ssrc:%u,%s]\n",ssrc,send.GetLastError());
	}

	//Store candidate
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);

	//Check if we are using transport wide for this packet
	if (packet->HasTransportWideCC() && senderSideEstimationEnabled)
		//Send packet and update stats in callback
		sender->Send(candidate, std::move(buffer), [
				weak = std::weak_ptr<SendSideBandwidthEstimation>(senderSideBandwidthEstimator),
				stats = PacketStats::Create(packet, len, now)
			](std::chrono::milliseconds now) mutable {
				//Get shared pointer from weak reference
				auto senderSideBandwidthEstimator = weak.lock();
				//If already gone
				if (!senderSideBandwidthEstimator)
					//Ignore
					return;
				//Update sent timestamp
				stats.timestamp = now.count();
				//Add new stat
				senderSideBandwidthEstimator->SentPacket(stats);
			}
		);
	else
		//Send packet
		sender->Send(candidate, std::move(buffer));

	//Get time
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
	
	DWORD bitrate   = 0;
	DWORD estimated = 0;
	DWORD probing	= 0;
		
	//Update source
	source.Update(now/1000, packet, len);
		
	//Get bitrates
	bitrate   = static_cast<DWORD>(source.acumulator.GetInstantAvg()*8);
	estimated = source.remb;
	probing	  = static_cast<DWORD>(probingBitrate.GetInstantAvg()*8);
	
	//Check if we need to send SR (1 per second)
	if (now-source.lastSenderReport>1E6)
		//Create and send rtcp sender retpor
		Send(RTCPCompoundPacket::Create(group->media.CreateSenderReport(now)));
	
	//Check if this packets support rtx
	bool rtx = group->rtx.ssrc && sendMaps.apt.GetTypeForCodec(packet->GetPayloadType())!=RTPMap::NotFound;
	
	//Do we need to send probing as inline media?
	if (!rtx && probe && group->type == MediaFrame::Video && packet->GetMark() && estimated>bitrate && probing<maxProbingBitrate)
	{
		BYTE size = 255;
		//Get probe padding needed
		DWORD probingBitrate = std::min(estimated-bitrate,maxProbingBitrate);

		//Get number of probes, do not send more than 32 continoues packets (~aprox 2mpbs)
		BYTE num = std::min<QWORD>((probingBitrate*33)/(8000*size),32);

		//UltraDebug("-DTLSICETransport::Run() | Sending inband probing packets [at:%u,estimated:%u,bitrate:%u,probing:%u,max:%u,num:%d]\n", packet->GetSeqNum(), estimated, bitrate,probingBitrate,maxProbingBitrate, num, sleep);

		//Set all the probes
		for (BYTE i=0;i<num;++i)
			//Send probe packet
			SendProbe(group,size);
	}
	
	//If packets supports rtx
	if (rtx)
		//Append it to the end of the packet history
		history.push_back(packet);
	
	return true;
}

void DTLSICETransport::onRTCP(const RTCPCompoundPacket::shared& rtcp)
{
	TRACE_EVENT("rtp", "DTLSICETransport::onRTCP", "size", rtcp->GetSize());

	//Get current time
	uint64_t now = getTime();
	
	//For each packet
	for (DWORD i = 0; i<rtcp->GetPacketCount();i++)
	{
		//Get pacekt
		auto packet = rtcp->GetPacket(i);
		//Check packet type
		switch (packet->GetType())
		{
			case RTCPPacket::SenderReport:
			{
				//Get sender report
				auto sr = std::static_pointer_cast<RTCPSenderReport>(packet);
				
				//Get ssrc
				DWORD ssrc = sr->GetSSRC();

				TRACE_EVENT("rtp", "DTLSICETransport::onRTCP::SR", "size", sr->GetSize(), "ssrc", ssrc);

				//Get source
				RTPIncomingSource* source = GetIncomingSource(ssrc);
				
				//If not found
				if (!source)
				{
					Warning("-DTLSICETransport::onRTCP() | Could not find incoming source for RTCP SR [ssrc:%u]\n",ssrc);
					rtcp->Dump();
					continue;
				}
				
				//Update source
				source->Process(now, sr);
				
				//Process all the Sender Reports
				for (DWORD j=0;j<sr->GetCount();j++)
				{
					//Get report
					auto report = sr->GetReport(j);
					//Check ssrc
					DWORD ssrc = report->GetSSRC();
					
					//Get group
					RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
					//If found
					if (group)
					{
						//Get media
						RTPOutgoingSource* source = group->GetSource(ssrc);
						//Check ssrc
						if (source)
						{
							RTPOutgoingSource* source = group->GetSource(ssrc);
							//Check we have it
							if (source)
							{
								//Process report
								if (source->ProcessReceiverReport(now/1000, report))
									//We need to update rtt
									SetRTT(source->rtt, now);
							}
						}
					}
				}
				break;
			}
			case RTCPPacket::ReceiverReport:
			{
				//Get receiver report
				auto rr = std::static_pointer_cast<RTCPReceiverReport>(packet);

				TRACE_EVENT("rtp", "DTLSICETransport::onRTCP::RR", "size", rr->GetSize(), "count", rr->GetCount());

				//Process all the receiver Reports
				for (DWORD j=0;j<rr->GetCount();j++)
				{
					//Get report
					auto report = rr->GetReport(j);
					//Check ssrc
					DWORD ssrc = report->GetSSRC();
					
					//Get group
					RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
					//If found
					if (group)
					{
						//Get media
						RTPOutgoingSource* source = group->GetSource(ssrc);
						//Check ssrc
						if (source)
						{
							RTPOutgoingSource* source = group->GetSource(ssrc);
							//Check we have it
							if (source)
							{
								//Process report
								if (source->ProcessReceiverReport(now/1000, report))
									//We need to update rtt
									SetRTT(source->rtt,now);
							}
						}
					}
				}
				break;
			}
			case RTCPPacket::ExtendedJitterReport:
				break;
			case RTCPPacket::SDES:
				break;
			case RTCPPacket::Bye:
			{
				//Get bye
				auto bye = std::static_pointer_cast<RTCPBye>(packet);
				//For each ssrc
				for (auto& ssrc : bye->GetSSRCs())
				{
					//Get media
					RTPIncomingSourceGroup* group = GetIncomingSourceGroup(ssrc);

					//Debug
					Debug("-DTLSICETransport::onRTCP() | Got BYE [ssrc:%u,group:%p,this:%p]\n", ssrc, group, this);

					//If found
					if (group)
						//Reset it
						group->Bye(ssrc);
				}
				break;
			}
			case RTCPPacket::App:
				break;
			case RTCPPacket::RTPFeedback:
			{
				//Get feedback packet
				auto fb = std::static_pointer_cast<RTCPRTPFeedback>(packet);
				//Get SSRC for media
				DWORD ssrc = fb->GetMediaSSRC();

				TRACE_EVENT("rtp", "DTLSICETransport::onRTCP::FB", "size", fb->GetSize(), "ssrc", ssrc);

				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPRTPFeedback::NACK:
					{
						//Get media
						RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
						//If not found
						if (!group)
						{
							//Dump
							fb->Dump();
							//Debug
							Warning("-DTLSICETransport::onRTCP() | Got NACK feedback message for unknown media  [ssrc:%u]\n", ssrc);
							//Ups! Skip
							break;
						}
						for (DWORD i = 0; i < fb->GetFieldCount(); i++)
						{
							//Get field
							auto field = fb->GetField<RTCPRTPFeedback::NACKField>(i);
							//Resent it
							ReSendPacket(group, field->pid);
							//Check each bit of the mask
							for (BYTE i = 0; i < 16; i++)
								//Check it bit is present to rtx the packets
								if ((field->blp >> i) & 1)
									//Resent it
									ReSendPacket(group, field->pid + i + 1);
						}
						break;
					}
					case RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest:
						UltraDebug("-DTLSICETransport::onRTCP() | TempMaxMediaStreamBitrateRequest\n");
						break;
					case RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification:
						UltraDebug("-DTLSICETransport::onRTCP() | TempMaxMediaStreamBitrateNotification\n");
						break;
					case RTCPRTPFeedback::TransportWideFeedbackMessage:
						//If sender side estimation is enabled
						if (senderSideEstimationEnabled)
							//Get each fiedl
							for (DWORD i=0;i<fb->GetFieldCount();i++)
							{
								//Get field
								auto field = fb->GetField<RTCPRTPFeedback::TransportWideFeedbackMessageField>(i);
								//Pass it to the estimator
								senderSideBandwidthEstimator->ReceivedFeedback(field->feedbackPacketCount,field->packets,now);
							}
						break;
					case RTCPRTPFeedback::UNKNOWN:
						UltraDebug("-DTLSICETransport::onRTCP() | RTCPRTPFeedback type unknown\n");
						break;
				}
				break;
			}
			case RTCPPacket::PayloadFeedback:
			{
				//Get feedback packet
				auto fb = std::static_pointer_cast<RTCPPayloadFeedback>(packet);
				//Get SSRC for media
				DWORD ssrc = fb->GetMediaSSRC();

				TRACE_EVENT("rtp", "DTLSICETransport::onRTCP::PFB", "size", fb->GetSize(), "ssrc", ssrc);

				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPPayloadFeedback::PictureLossIndication:
					case RTCPPayloadFeedback::FullIntraRequest:
					{
						//Get media
						RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
						
						//Debug
						Debug("-DTLSICETransport::onRTCP() | FPU requested [ssrc:%u,group:%p,this:%p]\n",ssrc,group,this);
						
						//If not found
						if (!group)
						{
							//Dump
							fb->Dump();
							//Debug
							Warning("-Got feedback message for unknown media  [ssrc:%u]\n",ssrc);
							//Ups! Skip
							continue;
						}
						//Call listeners
						group->onPLIRequest(ssrc);
						break;
					}
					case RTCPPayloadFeedback::SliceLossIndication:
						Debug("-DTLSICETransport::onRTCP() | SliceLossIndication\n");
						break;
					case RTCPPayloadFeedback::ReferencePictureSelectionIndication:
						Debug("-DTLSICETransport::onRTCP() | ReferencePictureSelectionIndication\n");
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffRequest:
						Debug("-DTLSICETransport::onRTCP() | TemporalSpatialTradeOffRequest\n");
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffNotification:
						Debug("-DTLSICETransport::onRTCP() | TemporalSpatialTradeOffNotification\n");
						break;
					case RTCPPayloadFeedback::VideoBackChannelMessage:
						Debug("-DTLSICETransport::onRTCP() | VideoBackChannelMessage\n");
						break;
					case RTCPPayloadFeedback::ApplicationLayerFeeedbackMessage:
						//For all message fields
						for (DWORD i=0;i<fb->GetFieldCount();i++)
						{
							//Get feedback
							auto msg = fb->GetField<RTCPPayloadFeedback::ApplicationLayerFeeedbackField>(i);
							//Get size and payload
							DWORD len		= msg->GetLength();
							const BYTE* payload	= msg->GetPayload();
							//Check if it is a REMB
							if (len>8 && payload[0]=='R' && payload[1]=='E' && payload[2]=='M' && payload[3]=='B')
							{
								//Get SSRC count
								BYTE num = payload[4];
								//GEt exponent
								BYTE exp = payload[5] >> 2;
								DWORD mantisa = payload[5] & 0x03;
								mantisa = mantisa << 8 | payload[6];
								mantisa = mantisa << 8 | payload[7];
								//Get bitrate
								DWORD bitrate = mantisa << exp;
								//For each
								for (DWORD i=0;i<num;++i)
								{
									//Check length
									if (len<8+4*i+4)
										//wrong format
										break;
									//Get ssrc
									DWORD target = get4(payload,8+4*i);
									//Get media
									RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(target);

									//Debug
									Debug("-DTLSICETransport::onRTCP() | REMB received [bitrate:%d,target:%u,group:%p,this:%p]\n", bitrate, target, group, this);
									
									//If found
									if (group)
										//Call listener
										group->onREMB(target,bitrate);
								}
							}
						}
						break;
					case RTCPPayloadFeedback::UNKNOWN:
						Debug("-DTLSICETransport::onRTCP() | RTCPPayloadFeedback type unknown\n");
						break;
				}
				break;
			}
			case RTCPPacket::FullIntraRequest:
				//THis is deprecated
				Debug("-DTLSICETransport::onRTCP() | FullIntraRequest!\n");
				break;
			case RTCPPacket::NACK:
				//THis is deprecated
				Debug("-DTLSICETransport::onRTCP() | NACK!\n");
				break;
		}
	}
}



RTPIncomingSourceGroup* DTLSICETransport::GetIncomingSourceGroup(DWORD ssrc)
{
	//Get the incouming source
	auto it = incoming.find(ssrc);
				
	//If not found
	if (it==incoming.end())
		//Not found
		return NULL;
	
	//Get source froup
	return it->second;
}

RTPIncomingSource* DTLSICETransport::GetIncomingSource(DWORD ssrc)
{
	//Get the incouming source
	auto it = incoming.find(ssrc);
				
	//If not found
	if (it==incoming.end())
		//Not found
		return NULL;
	
	//Get source
	return it->second->GetSource(ssrc);

}

RTPOutgoingSourceGroup* DTLSICETransport::GetOutgoingSourceGroup(DWORD ssrc)
{
	//Get the incouming source
	auto it = outgoing.find(ssrc);
				
	//If not found
	if (it==outgoing.end())
		//Not found
		return NULL;
	
	//Get source froup
	return it->second;

}

RTPOutgoingSource* DTLSICETransport::GetOutgoingSource(DWORD ssrc)
{
	//Get the incouming source
	auto it = outgoing.find(ssrc);
				
	//If not found
	if (it==outgoing.end())
		//Not found
		return NULL;
	
	//Get source
	return it->second->GetSource(ssrc);
}

void DTLSICETransport::SetRTT(DWORD rtt, QWORD now)
{
	//Debug
	UltraDebug("-DTLSICETransport::SetRTT() [rtt:%d]\n",rtt);
	//Sore it
	this->rtt = rtt;
	//Update jitters
	for (auto it : incoming)
		//Update jitter
		it.second->SetRTT(rtt,now/1000);
	//If sse is enabled
	if (senderSideEstimationEnabled)
		//Add estimation
		senderSideBandwidthEstimator->UpdateRTT(now,rtt);
}

void DTLSICETransport::SendTransportWideFeedbackMessage(DWORD ssrc)
{
	//Debug
	//UltraDebug("-DTLSICETransport::SendTransportWideFeedbackMessage() [ssrc:%d]\n", ssrc);
	//RTCP packet
	auto rtcp = RTCPCompoundPacket::Create();

	//Create rtcp transport wide feedback
	auto feedback = rtcp->CreatePacket<RTCPRTPFeedback>(RTCPRTPFeedback::TransportWideFeedbackMessage,mainSSRC,ssrc);

	//Create trnasport field
	auto field = feedback->CreateField<RTCPRTPFeedback::TransportWideFeedbackMessageField>(++feedbackPacketCount);

	//Proccess and delete all elements
	for (auto it =transportWideReceivedPacketsStats.cbegin();
		  it!=transportWideReceivedPacketsStats.cend();
		  it = transportWideReceivedPacketsStats.erase(it))
	{
		//Get stat
		auto& stats = it->second;
		//Get transport seq
		DWORD transportExtSeqNum = it->first;
		//Get relative time
		QWORD time = stats.time - initTime;

		//if not first and not out of order
		if (lastFeedbackPacketExtSeqNum && transportExtSeqNum>lastFeedbackPacketExtSeqNum)
			//For each lost
			for (DWORD i = lastFeedbackPacketExtSeqNum+1; i<transportExtSeqNum; ++i)
				//Add it
				field->packets.insert(std::make_pair(i,0));

		//Store last
		lastFeedbackPacketExtSeqNum = transportExtSeqNum;

		//Add this one
		field->packets.insert(std::make_pair(transportExtSeqNum,time));
	}

	//Send packet
	Send(rtcp);
}

void DTLSICETransport::Start()
{
	TRACE_EVENT("transport", "DTLSICETransport::Start");

	Debug("-DTLSICETransport::Start()\n");
	
	//Init DTLS
	dtls.Init();

	//Get init time
	initTime = getTime();
	dcOptions.localPort = 5000;
	dcOptions.remotePort = 5000;
	//Run ice timeout timer
	iceTimeoutTimer = timeService.CreateTimer(IceTimeout,[this](auto now){
		//Log
		Debug("-DTLSICETransport::onIceTimeoutTimer() ICE timeout\n");
		//If got listener
		if (listener)
			//Fire timeout 
			listener->onICETimeout();
	});
	//Set name for debug
	iceTimeoutTimer->SetName("DTLSICETransport - ice timeout");
	//Create new probe timer
	probingTimer = timeService.CreateTimer([this](std::chrono::milliseconds ms) {
		//Do probe
		Probe(ms.count());
		});
	//Set name for debug
	probingTimer->SetName("DTLSICETransport - bwe probe");
	//Create sse timer
	sseTimer = timeService.CreateTimer([this](std::chrono::milliseconds ms) {
		//Send feedback now
		SendTransportWideFeedbackMessage(lastMediaSSRC);
	});
	//Set name for debug
	sseTimer->SetName("DTLSICETransport - twcc feedback");
	//Start
	endpoint.Init(dcOptions);
	//Started
	started = true;
}

void DTLSICETransport::Stop()
{
	if (!started)
		return;
	
	//Log
	TRACE_EVENT("transport", "DTLSICETransport::Stop");
	Debug(">DTLSICETransport::Stop()\n");
	
	//Check probing timer
	if (probingTimer)
	{
		//Stop probing
		probingTimer->Cancel();
		//Remove timer
		probingTimer.reset();
	}

	//Check sse timer
	if (sseTimer)
	{
		//Stop probing
		sseTimer->Cancel();
		//Remove timer
		sseTimer.reset();
	}
	
	//Check ice timeout timer
	if (iceTimeoutTimer)
		//Stop probing
		iceTimeoutTimer->Cancel();

	//Stop
	endpoint.Close();

	//Send dtls shutdown
	dtls.Reset();

	//End DTLS as well
	dtls.End();

	//No active candiadte
	active = nullptr;
	
	//Not started anymore
	started = false;
	
	//Log
	Debug("<DTLSICETransport::Stop()\n");
}

int DTLSICETransport::Enqueue(const RTPPacket::shared& packet)
{
	//Trace
	TRACE_EVENT("rtp", "DTLSICETransport::Enqueue RTP",
		"ssrc", packet->GetSSRC(),
		"seqNum", packet->GetSeqNum());

	//TODO: check if we are actuall sending from a different thread ocasionally
	//Send async
	//timeService.Async([=](auto now){
		//Send
		Send(packet);
	//});
	
	return 1;
}

void DTLSICETransport::Probe(QWORD now)
{
	TRACE_EVENT("transport", "DTLSICETransport::Probe", "now", now);

	//Ensure that transport wide cc is enabled
	if (senderSideEstimationEnabled && probe && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
	{
		//Update bitrates
		outgoingBitrate.Update(now);
		probingBitrate.Update(now);
		//Calculate sleep time
		uint64_t sleep = lastProbe ? std::min<uint64_t>(now - lastProbe, probingTimer->GetRepeat().count()) : probingTimer->GetRepeat().count();
		
		//Get bitrates
		DWORD bitrate		= static_cast<DWORD>(outgoingBitrate.GetInstantAvg()*8);
		DWORD probing		= static_cast<DWORD>(probingBitrate.GetInstantAvg()*8);
		DWORD target		= senderSideBandwidthEstimator->GetAvailableBitrate();
		DWORD limit		= std::min(target, probingBitrateLimit);

		//Log(">DTLSICETransport::Probe() | [target:%ubps,bitrate:%ubps,probing:%ubps,history:%d,probingBitrateLimit=%ubps,maxProbingBitrate=%ubps]\n",target,bitrate,probing,history.size(),probingBitrateLimit,maxProbingBitrate);
			
		//If we can still send more
		if (bitrate<limit && probing<maxProbingBitrate)
		{
			//Calculate how much probe bitrate should we sent
			DWORD probe = std::min<DWORD>(limit - bitrate, maxProbingBitrate);

			//Get size of probes
			DWORD probeSize = probe*sleep/8000;
			
			//Log("-DTLSICETransport::Probe() | Sending probe packets [target:%ubps,bitrate:%ubps,limit:%ubps,probe:%u,probingSize:%d,sleep:%d]\n", target,  bitrate, limit, probe, probeSize, sleep);

			//If we have packet history
			if (!history.empty())
			{
				int found = true;
				//Get first packet
				auto smallest = history.front();

				//Sent until no more probe
				while (probeSize && found)
				{	
					//We need to always send one at minimun
					found = false;
					//For each other packet in history
					for (size_t i=1; i<history.length(); ++i)
					{
						//Get candidate
						auto candidate = history.at(i);

						//Don't send too much data
						if (candidate->GetMediaLength()>probeSize)
						{
							if (candidate->GetMediaLength() < smallest->GetMediaLength())
								smallest = candidate;
							//Try next
							continue;
						}
						//Send probe packet
						DWORD len = SendProbe(candidate);
						//Check len
						if (!len)
							//Error
							return;
						//Update probing
						probingBitrate.Update(now,len);
						//Check size
						if (len>probeSize)
							//Done
							break;
						//Reduce probe
						probeSize -= len;
						found = true;
					}
				}
				//If we have not found any packet
				if (!found)
				{
					//Send the smallest one
					DWORD len = SendProbe(smallest);
					//Check len
					if (!len)
						//Done
						return;
					//Update probing
					probingBitrate.Update(now, len);
				}
			} else {
				//Ensure we send at least one packet
				DWORD size = std::min(255u, probeSize);
				//Check if we have an outgpoing group
				for (auto &group : outgoing)
				{
					//We can only probe on rtx with video
					if (group.second->type == MediaFrame::Video)
					{
						//Set all the probes
						while (probeSize >=size)
						{
							//Send probe packet
							DWORD len = SendProbe(group.second,size);
							//Check len
							if (!len)
								//Done
								return;
							//Update probing
							probingBitrate.Update(now,len);
							//Check size
							if (len>probeSize)
								//Done
								return;
							//Reduce probe
							probeSize -= len;
						}
						//Done
						return;
					}
				}
			}
		}
		//Update bitrates
		//bitrate = static_cast<DWORD>(outgoingBitrate.GetInstantAvg() * 8);
		//probing = static_cast<DWORD>(probingBitrate.GetInstantAvg() * 8);
		//Log
		//Log("<DTLSICETransport::Probe() | [target:%ubps,bitrate:%ubps,probing:%ubps]\n", target, bitrate, probing);
	}
	//Update last probe time
	lastProbe = now;
}

void DTLSICETransport::SetListener(const Listener::shared& listener)
{
	Debug(">DTLSICETransport::SetListener() [this:%p,listener:%p]\n", this, listener.get());
	//Add in main thread async
	timeService.Async([=](auto now){
		//Store listener
		this->listener = listener;
	});
}

void DTLSICETransport::SetState(DTLSState state)
{
	//Store state
	this->state = state;
	//If got listener
	if (listener)
		//Fire change
		listener->onDTLSStateChanged(state);
}


void DTLSICETransport::SetRemoteOverrideBWE(bool overrideBWE)
{
	//Log
	Debug("-DTLSICETransport::SetRemoteOverrideBWE() [override:%d]\n", overrideBWE);
	this->overrideBWE = overrideBWE; 
}

void DTLSICETransport::SetRemoteOverrideBitrate(DWORD bitrate) 
{ 
	//Log
	Debug("-DTLSICETransport::SetRemoteOverrideBitrate() [bitrate:%d]\n", bitrate);
	this->remoteOverrideBitrate = bitrate; 
}


void DTLSICETransport::DisableREMB(bool disabled)
{
	//Log
	Debug("-DTLSICETransport::DisableREMB() [disabled:%d]\n", disabled);
	this->disableREMB = disabled;
}
void DTLSICETransport::SetMaxProbingBitrate(DWORD bitrate)
{
	//Log
	Debug("-DTLSICETransport::SetMaxProbingBitrate() [bitrate:%d]\n", bitrate);
	this->maxProbingBitrate = bitrate;
}

void DTLSICETransport::SetProbingBitrateLimit(DWORD bitrate)
{
	//Log
	Debug("-DTLSICETransport::SetProbingBitrateLimit() [bitrate:%d]\n", bitrate);
	this->probingBitrateLimit = bitrate;
}

void DTLSICETransport::SetBandwidthProbing(bool probe)
{
	//Log
	Debug("-DTLSICETransport::SetBandwidthProbing() [probe:%d]\n", probe);
	//Set probing status
	this->probe = probe;

	//Check if we need to start the timar
	CheckProbeTimer();
}


void DTLSICETransport::EnableSenderSideEstimation(bool enabled)
{ 
	//Log
	Debug("-DTLSICETransport::EnableSenderSideEstimation() [enabled:%d]\n", enabled);
	//Update flag
	this->senderSideEstimationEnabled = enabled;

	//Check if we need to start the timer
	CheckProbeTimer();
}


void DTLSICETransport::CheckProbeTimer()
{
	Debug("-DTLSICETransport::CheckProbeTimer() | [probingTimer:%d]\n",!!probingTimer);

	//Dispatch to the event loop thread
	timeService.Async([=](auto now) {
		//If we don't have timer anumore
		if (!probingTimer)
			//Do nothing
			return;
		//No video
		bool video = false;
		//Check if there is any video source
		for (const auto& [ssrc,outgoing] : outgoing)
		{
			//If it is video
			if (outgoing->type == MediaFrame::Video)
			{
				//Got one
				video = true;
				break;
			}
		}
	
		//Check if we have to start if
		if (this->senderSideEstimationEnabled && this->probe && video && state == DTLSState::Connected)
		{
			//If not already started
			if (!probingTimer->IsScheduled())
				//Start probing timer again
				probingTimer->Repeat(ProbingInterval);
		} else {
			//Stop probing
			probingTimer->Cancel();
		}
	});
}