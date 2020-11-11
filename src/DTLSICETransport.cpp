/* 
 * File:   RTPICETransport.cpp
 * Author: Sergio
 * 
 * Created on 8 de enero de 2017, 18:37
 */
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

constexpr auto IceTimeout = 30000ms;

DTLSICETransport::DTLSICETransport(Sender *sender,TimeService& timeService) :
	sender(sender),
	timeService(timeService),
	endpoint(timeService),
	dtls(*this,timeService,endpoint.GetTransport()),
	incomingBitrate(250),
	outgoingBitrate(250),
	rtxBitrate(250),
	probingBitrate(250)
{
}

DTLSICETransport::~DTLSICETransport()
{
	//Reset & stop
	Reset();
	Stop();
}

void DTLSICETransport::onDTLSPendingData()
{
	//UltraDebug(">DTLSConnection::onDTLSPendingData() [active:%p]\n",active);
	//Until depleted
	while(active)
	{
		Packet buffer;
		//Read from dtls
		size_t len = dtls.Read(buffer.GetData(),buffer.GetCapacity());
		//Check result
		if (len<=0)
			break;
		//Set read size
		buffer.SetSize(len);
		//Send
		Log("-DTLSConnection::onDTLSPendingData() | dtls send [len:%d]\n",len);
		//Send it back
		sender->Send(active,std::move(buffer));
		//Update bitrate
		outgoingBitrate.Update(getTimeMS(),len);
	}
	//UltraDebug("<DTLSConnection::onDTLSPendingData() | no more data\n");
}

int DTLSICETransport::onData(const ICERemoteCandidate* candidate,const BYTE* data,DWORD size)
{
	//Acumulate bitrate
	incomingBitrate.Update(getTimeMS(),size);
	
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

		//Check session
		if (!recv.IsSetup())
			return Warning("-DTLSICETransport::onData() |  Recv SRTPSession is not setup\n");

		//unprotect
		size_t len = recv.UnprotectRTCP(data,size);
		
		//Check size
		if (!len)
			//Error
			return Warning("-DTLSICETransport::onData() | Error unprotecting rtcp packet [%s]\n",recv.GetLastError());

		//If dumping
		if (dumper && dumpRTCP)
			//Write udp packet
			dumper->WriteUDP(getTimeMS(),candidate->GetIPAddress(),candidate->GetPort(),0x7F000001,5004,data,len);

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
	size_t len = recv.UnprotectRTP(data,size);
	//Check status
	if (!len)
		//Error
		return Warning("-DTLSICETransport::onData() | Error unprotecting rtp packet [%s]\n",recv.GetLastError());
	
	//Parse rtp packet
	RTPPacket::shared packet = RTPPacket::Parse(data,len,recvMaps.rtp,recvMaps.ext);
	
	//Check
	if (!packet)
		//Error
		return Warning("-DTLSICETransport::onData() | Could not parse rtp packet\n");
	
	//If dumping
	if (dumper && dumpInRTP)
	{
		//Get truncate size
		DWORD truncate = dumpRTPHeadersOnly ? len - packet->GetMediaLength() + 16 : 0;
		//Write udp packet
		dumper->WriteUDP(getTimeMS(),candidate->GetIPAddress(),candidate->GetPort(),0x7F000001,5004,data,len, truncate);
	}
	
	//Get ssrc
	DWORD ssrc = packet->GetSSRC();
	
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
		if (!rid.empty() && packet->GetCodec()==VideoCodec::RTX)
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
		} else if (packet->HasMediaStreamId() && packet->GetCodec()==VideoCodec::RTX) {
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
	
	//UltraDebug("-Got RTP on media:%s sssrc:%u seq:%u pt:%u codec:%s rid:'%s', mid:'%s'\n",MediaFrame::TypeToString(group->type),ssrc,header.sequenceNumber,header.payloadType,GetNameForCodec(group->type,codec),group->rid.c_str(),group->mid.c_str());
	
	//Process packet and get source
	RTPIncomingSource* source = group->Process(packet);
	
	//Ensure it has a source
	if (!source)
		//error
		return Warning("-DTLSICETransport::onData() | Group does not contain ssrc [%u]\n",ssrc);

	//If transport wide cc is used
	if (packet->HasTransportWideCC())
	{
		// Get current seq mum
		WORD transportSeqNum = packet->GetTransportSeqNum();
		
		//Get max seq num so far, it is either last one if queue is empy or last one of the queue
		DWORD maxFeedbackPacketExtSeqNum = transportWideReceivedPacketsStats.size() ? transportWideReceivedPacketsStats.rbegin()->first : lastFeedbackPacketExtSeqNum;
		
		//Check if we have a sequence wrap
		if (transportSeqNum<0x00FF && (maxFeedbackPacketExtSeqNum & 0xFFFF)>0xFF00)
		{
			//Increase cycles
			feedbackCycles++;
			//If looping
			if (feedbackCycles)
				//Send feedback now
				SendTransportWideFeedbackMessage(ssrc);
		}

		//Get extended value
		DWORD transportExtSeqNum = feedbackCycles<<16 | transportSeqNum;
		
		//Get current time
		auto now = getTime();
		
		//Add packets to the transport wide stats
		transportWideReceivedPacketsStats[transportExtSeqNum] = PacketStats::Create(packet,size,now);
		
		//If we have enought or timeout (500ms)
		if (packet->GetMark()  || transportWideReceivedPacketsStats.size()>100 || (now-transportWideReceivedPacketsStats.begin()->second->time)>5E5)
			//Send feedback message
			SendTransportWideFeedbackMessage(ssrc);
	}

	//If it was an RTX packet and not a padding only one
	if (ssrc==group->rtx.ssrc && packet->GetMediaLength()) 
	{
		//Ensure that it is a RTX codec
		if (codec!=VideoCodec::RTX)
			//error
			return  Warning("-DTLSICETransport::onData() | No RTX codec on rtx sssrc:%u type:%d codec:%d\n",packet->GetSSRC(),packet->GetPayloadType(),packet->GetCodec());
	
		//Find apt type
		auto apt = recvMaps.apt.GetCodecForType(packet->GetPayloadType());
		//Find codec 
		codec = recvMaps.rtp.GetCodecForType(apt);
		//Check codec
		if (codec==RTPMap::NotFound)
			  //Error
			  return Warning("-DTLSICETransport::onData() | RTP RTX packet apt type unknown [%d]\n",MediaFrame::TypeToString(packet->GetMediaType()),packet->GetPayloadType());
		
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
	} else if (ssrc==group->fec.ssrc)  {
		//Ensure that it is a FEC codec
		if (codec!=VideoCodec::FLEXFEC)
			//error
			return  Warning("-DTLSICETransport::onData() | No FLEXFEC codec on fec sssrc:%u type:%d codec:%d\n",MediaFrame::TypeToString(packet->GetMediaType()),packet->GetPayloadType(),packet->GetSSRC());
		//DO NOTHING with it yet
		return 1;
	}
	//Check if we have receiver already an SR for media stream
	if ( group->media.lastReceivedSenderReport)
	{
		//Get ntp and rtp timestamps
		QWORD timestamp =  group->media.lastReceivedSenderRTPTimestampExtender.GetExtSeqNum();
		
		//IF packet is newer than SR (should be)
		if (timestamp<packet->GetExtTimestamp())
		{
			//Calculate sender time 
			QWORD senderTime =  group->media.lastReceivedSenderTime + (packet->GetExtTimestamp()-timestamp)*1000/packet->GetClockRate();
			//Set calculated sender time
			packet->SetSenderTime(senderTime);
		}
	}
	

	//Add packet and see if we have lost any in between
	int lost = group->AddPacket(packet,size);

	//Check if it was rejected
	if (lost<0)
		//Increase rejected counter
		source->dropPackets++;
	
	//Get current time
	auto now = getTime();
	
	if ( group->type == MediaFrame::Video && 
		( lost>0 ||  (group->GetCurrentLost() && (now-source->lastNACKed)/1000>fmax(rtt,20)))
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
		if (group->remoteBitrateEstimation)
		{
			
			//Add remb block
			DWORD bitrate = 0;
			std::list<DWORD> ssrcs;
			
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
			
			//LOg
			Debug("-DTLSICETransport::REMB() [ssrc:%x,mid:'%s',count:%d,bitrate:%u]\n",group->media.ssrc,group->mid.c_str(),ssrcs.size(),bitrate);
			
			// SSRC of media source (32 bits):  Always 0; this is the same convention as in [RFC5104] section 4.2.2.2 (TMMBN).
			auto remb = rtcp->CreatePacket<RTCPPayloadFeedback>(RTCPPayloadFeedback::ApplicationLayerFeeedbackMessage,group->media.ssrc,WORD(0));
			//Send estimation
			remb->AddField(RTCPPayloadFeedback::ApplicationLayerFeeedbackField::CreateReceiverEstimatedMaxBitrate(ssrcs,bitrate));
		}
		
		//If there is no outgoing stream
		if (outgoing.empty() && group->rtx.ssrc)
		{
			//We try to calculate rtt based on rtx
			auto nack = rtcp->CreatePacket<RTCPRTPFeedback>(RTCPRTPFeedback::NACK,mainSSRC,group->media.ssrc);
			//Get last seq num for calculating rtt based on rtx
			WORD last = group->SetRTTRTX(now);
			//Request it
			nack->AddField(std::make_shared<RTCPRTPFeedback::NACKField>(last,0));
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
		return Error("-DTLSICETransport::SendProbe() | No rtx or apt found\n");
	
	//Get rtx source
	RTPOutgoingSource& source = group->rtx;
	
	//Sync
	{
		//Lock in scope
		ScopedLock scope(source);
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
	
	//Send buffer
	Packet buffer;
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize data
	int len = packet->Serialize(data,size,sendMaps.ext);
	
	//IF failed
	if (!len)
		return Warning("-DTLSICETransport::SendProbe() | Could not serialize packet\n");

	//If we don't have an active candidate yet
	if (!active)
		//Error
		return Warning("-DTLSICETransport::SendProbe() | We don't have an active candidate yet\n");

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
		//Error
		return Error("-DTLSICETransport::SendProbe() | Error protecting RTP packet [ssrc:%u,%s]\n",source.ssrc,send.GetLastError());
	
	//Store candidate before unlocking
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);
	//No error yet, send packet
	sender->Send(candidate,std::move(buffer));
	
	//Update current time after sending
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
		
        //Synchronized
	{
		//Block scope
		ScopedLock scope(source);
                //Update last send time
                source.lastTime		= packet->GetTimestamp();
                source.lastPayloadType  = packet->GetPayloadType();
	
                //Update stats
                source.Update(now/1000,packet->GetSeqNum(),len);
        }
	
	//Add to transport wide stats
	if (packet->HasTransportWideCC())
	{
		//Create stats
		auto stats = PacketStats::Create(packet,len,now);
		//It is probe
		stats->probing = true;
		//Add new stat
		senderSideBandwidthEstimator.SentPacket(stats);
	}
	
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
                //Lock in scope
                ScopedLock scope(source);
		//Update RTX headers
		header.ssrc		= source.ssrc;
		header.payloadType	= sendMaps.apt.begin()->first;
		header.sequenceNumber	= extSeqNum = source.NextSeqNum();
		header.timestamp	= source.lastTime++;
		//Padding
		header.padding		= 1;
	} else {
                //Lock in scope
                ScopedLock scope(source);
		//Update normal headers
		header.ssrc		= source.ssrc;
		header.payloadType	= source.lastPayloadType;
		header.sequenceNumber	= extSeqNum = source.AddGapSeqNum();
		header.timestamp	= source.lastTime;
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
	
	//Send buffer
	Packet buffer;
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	int	len  = 0;

	//Serialize header
	int n = header.Serialize(data,size);

	//Comprobamos que quepan
	if (!n)
		//Error
		return Error("-DTLSICETransport::SendProbe() | Error serializing rtp headers\n");

	//Inc len
	len += n;

	//If we have extension
	if (header.extension)
	{
		//Serialize
		n = extension.Serialize(sendMaps.ext,data+len,size-len);
		//Comprobamos que quepan
		if (!n)
			//Error
			return Error("-DTLSICETransport::SendProbe() | Error serializing rtp extension headers\n");
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
		//Error
		return Debug("-DTLSICETransport::SendProbe() | We don't have an active candidate yet\n");

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
		//Error
		return Error("-RTPTransport::SendProbe() | Error protecting RTP packet [ssrc:%u,%s]\n",source.ssrc,send.GetLastError());

	//Store candidate before unlocking
	ICERemoteCandidate* candidate = active;
	//Set buffer size
	buffer.SetSize(len);
	//No error yet, send packet
	sender->Send(candidate,std::move(buffer));
	
	//Update now
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
	
        //SYNC
        {
                //Lock in scope
                ScopedLock scope(source);
                //Update last send time
                source.lastTime		= header.timestamp;
                source.lastPayloadType  = header.payloadType;
                //Update stats
                source.Update(now/1000,header.sequenceNumber,len);
        }
	
	
	//Add to transport wide stats
	if (extension.hasTransportWideCC)
	{
		//Create stat
		auto stats = PacketStats::Create(
			extension.transportSeqNum,
			header.ssrc,
			extSeqNum,
			len,
			0,
			header.timestamp,
			now,
			false
		);
		//It is probe
		stats->probing = true;
		//Add new stat
		senderSideBandwidthEstimator.SentPacket(stats);
	}
	
	return len;
}

void DTLSICETransport::ReSendPacket(RTPOutgoingSourceGroup *group,WORD seq)
{
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
	{
		//Error
		Debug("-DTLSICETransport::ReSendPacket() | Send SRTPSession is not setup yet\n");
		//Done
		return;
	}
	
	UltraDebug("-DTLSICETransport::ReSendPacket() | resending [seq:%d,ssrc:&%u,rtx:%u]\n",seq,group->media.ssrc,group->rtx.ssrc);
	
	//Get current time
	auto now = getTime();
	
	//Update rtx bitrate
	rtxBitrate.Update(now/1000);
	
	//Get available bitrate
	DWORD availableBitrate = senderSideBandwidthEstimator.GetAvailableBitrate();
	
	//Check if we are sending way to much bitrate
	if (availableBitrate && rtxBitrate.GetInstantAvg()*8 > availableBitrate * 0.30f)
		//Error
		return (void)Debug("-DTLSICETransport::ReSendPacket() | Too much bitrate on rtx, skiping rtx:%lld estimated:%u available:%d\n",(uint64_t)(rtxBitrate.GetInstantAvg()*8), senderSideBandwidthEstimator.GetEstimatedBitrate(),availableBitrate);
	
	//Find packet to retransmit
	auto original = group->GetPacket(seq);

	//If we don't have it anymore
	if (!original)
		//Debug
		return (void)Warning("-DTLSICETransport::ReSendPacket() | packet not found[seq:%d,ssrc:&%u,rtx:%u]\n",seq,group->media.ssrc,group->rtx.ssrc);
	
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
                //Lock in scope
                ScopedLock scope(source);
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
	
	//Send buffer
	Packet buffer;
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize data
	int len = packet->Serialize(data,size,sendMaps.ext);
	
	//IF failed
	if (!len)
		return (void)Warning("-DTLSICETransport::ReSendPacket() | Could not serialize packet\n");

	//If we don't have an active candidate yet
	if (!active)
		//Error
		return (void)Warning("-DTLSICETransport::ReSendPacket() | We don't have an active candidate yet\n");

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
		//Error
		return (void)Error("-RTPTransport::ReSendPacket() | Error protecting RTP packet [ssrc:%u,%s]\n",source.ssrc,send.GetLastError());
	
	//Store candidate before unlocking
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);
	//No error yet, send packet
	sender->Send(candidate,std::move(buffer));
	
	//Update current time after sending
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
	rtxBitrate.Update(now/1000,len);
	
	//Get time for packets to discard, always have at least 200ms, max 500ms
	QWORD until = now/1000 - (200+fmin(rtt*2,300));
	
	//Release packets from rtx queue
	group->ReleasePackets(until);	
	
        //Synchronized
	{
		//Block scope
		ScopedLock scope(source);
                //Update last send time
                source.lastTime		= packet->GetTimestamp();
                source.lastPayloadType  = packet->GetPayloadType();
	
                //Update stats
                source.Update(now/1000,packet->GetSeqNum(),len);
        }
	
	//Check if we are using transport wide for this packet
	if (packet->HasTransportWideCC())
	{
		//Create stats
		auto stats = PacketStats::Create(packet,len,now);
		//It is rtx
		stats->rtx = true;
		//Add new stat
		senderSideBandwidthEstimator.SentPacket(stats);
	}
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
		//ADD it
		sendMaps.rtp[type] = codec;
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
		sendMaps.ext[id] = ext;
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
			sendMaps.rtp[type] = codec;
			//Get rtx and fec
			BYTE rtx = it->GetProperty("rtx",0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				sendMaps.rtp[rtx] = VideoCodec::RTX;
				//Add the reverse one
				sendMaps.apt[rtx] = type;
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
		sendMaps.ext[id] = ext;
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
		//ADD it
		recvMaps.rtp[type] = codec;
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
		recvMaps.ext[id] = ext;
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
			recvMaps.rtp[type] = codec;
			//Get rtx and fec
			BYTE rtx = it->GetProperty("rtx",0);
			//Check if it has rtx
			if (rtx)
			{
				//ADD it
				recvMaps.rtp[rtx] = VideoCodec::RTX;
				//Add the reverse one
				recvMaps.apt[rtx] = type;
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
		recvMaps.ext[id] = ext;
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
	timeService.Sync([&](...){
		//Check we are not dumping
		if (this->dumper)
		{
			//Error
			done = Error("-DTLSICETransport::Dump() | Already dumping\n");
			return;
		}

		//Store pcap as dumper
		this->dumper = dumper;

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
	timeService.Sync([&](...){
		//Check we are not dumping
		if (!this->dumper)
		{
			//Error
			done = Error("-DTLSICETransport::StopDump() | Not dumping\n");
			return;
		}

		//Close dumper
		this->dumper->Close();
		//Delete it
		delete(this->dumper);
		//Not dumping
		this->dumper = nullptr;
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
	timeService.Sync([&](...){
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
		this->dumper = pcap;

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
	return senderSideBandwidthEstimator.Dump(filename);
}

int DTLSICETransport::StopDumpBWEStats()
{
	return senderSideBandwidthEstimator.StopDump();
}


void DTLSICETransport::Reset()
{
	Log("-DTLSICETransport::Reset()\n");

	//Execute on timer thread
	timeService.Sync([=](...){
		//Clean mem
		if (iceLocalUsername)
			free(iceLocalUsername);
		if (iceLocalPwd)
			free(iceLocalPwd);
		if (iceRemoteUsername)
			free(iceRemoteUsername);
		if (iceRemotePwd)
			free(iceRemotePwd);
		
		//Reset srtp
		send.Reset();
		recv.Reset();

		//Check if dumping
		if (dumper)
		{
			//Close dumper
			dumper->Close();
			//Delete it
			delete(dumper);
		}
		//No ice
		iceLocalUsername = NULL;
		iceLocalPwd = NULL;
		iceRemoteUsername = NULL;
		iceRemotePwd = NULL;
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
	//Clean mem
	if (iceLocalUsername)
		free(iceLocalUsername);
	if (iceLocalPwd)
		free(iceLocalPwd);
	//Store values
	iceLocalUsername = strdup(username);
	iceLocalPwd = strdup(pwd);
	//Ok
	return 1;
}


int DTLSICETransport::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	Log("-DTLSICETransport::SetRemoteSTUNCredentials() |  [frag:%s,pwd:%s]\n",username,pwd);
	//Clean mem
	if (iceRemoteUsername)
		free(iceRemoteUsername);
	if (iceRemotePwd)
		free(iceRemotePwd);
	//Store values
	iceRemoteUsername = strdup(username);
	iceRemotePwd = strdup(pwd);
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
	//Init DTLS
	return dtls.Init();
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
	
	//Create new probe timer
	probingTimer = timeService.CreateTimer(0ms,5ms,[this](...){
		//Do probe
		Probe();
	});
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

bool DTLSICETransport::AddOutgoingSourceGroup(RTPOutgoingSourceGroup *group)
{
	//Log
	Log("-DTLSICETransport::AddOutgoingSourceGroup() [group:%p,ssrc:%u,fec:%u,rtx:%u]\n",group,group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//Done
	bool done = true;

	//Dispatch to the event loop thread
	timeService.Sync([&](...){
		//Get ssrcs
		const auto media = group->media.ssrc;
		const auto rtx   = group->rtx.ssrc;
		const auto fec	 = group->fec.ssrc;
		
		//Check they are not already assigned
		if (media && outgoing.find(media)!=outgoing.end())
		{
			//Error
			done = Error("-AddOutgoingSourceGroup media ssrc already assigned");
			return;
		}
		if (fec && outgoing.find(fec)!=outgoing.end())
		{
			//Error
			done = Error("-AddOutgoingSourceGroup fec ssrc already assigned");
			return;
		}
		if (rtx && outgoing.find(rtx)!=outgoing.end())
		{
			//Error
			done = Error("-AddOutgoingSourceGroup rtx ssrc already assigned");
			return;
		}

		//Add it for each group ssrc
		if (media)
		{
			outgoing[media] = group;
			send.AddStream(media);
		}
		if (fec)
		{
			outgoing[fec] = group;
			send.AddStream(fec);
		}
		if (rtx)
		{
			outgoing[rtx] = group;
			send.AddStream(rtx);
		}

		//If we don't have a mainSSRC
		if (mainSSRC==1 && media)
			//Set it
			mainSSRC = media;

		//Create rtcp sender retpor
		auto rtcp = RTCPCompoundPacket::Create(group->media.CreateSenderReport(getTime()));
		//Send packet
		Send(rtcp);	
	});
	
	//Done
	return done;
	
}

bool DTLSICETransport::RemoveOutgoingSourceGroup(RTPOutgoingSourceGroup *group)
{
	Log("-DTLSICETransport::RemoveOutgoingSourceGroup() [ssrc:%u,fec:%u,rtx:%u]\n",group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);

	//Dispatch to the event loop thread
	timeService.Sync([=](...){
		Log("-DTLSICETransport::RemoveOutgoingSourceGroup() | Async [ssrc:%u,fec:%u,rtx:%u]\n",group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
		//Get ssrcs
		std::vector<DWORD> ssrcs;
		const auto media = group->media.ssrc;
		const auto rtx   = group->rtx.ssrc;
		const auto fec	 = group->fec.ssrc;
		
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
	});
	
	//Done
	return true;
}

bool DTLSICETransport::AddIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-DTLSICETransport::AddIncomingSourceGroup() [mid:'%s',rid:'%s',ssrc:%u,fec:%u,rtx:%u]\n",group->mid.c_str(),group->rid.c_str(),group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc && group->rid.empty())
		return Error("No media ssrc or rid defined, stream will not be added\n");
	
	//The asycn result
	bool done = true;
	
	//Dispatch to the event loop thread
	timeService.Sync([&](...){
		//Get ssrcs
		const auto media = group->media.ssrc;
		const auto rtx   = group->rtx.ssrc;
		const auto fec	 = group->fec.ssrc;
		
		//Check they are not already assigned
		if (media && incoming.find(media)!=incoming.end())
		{
			//Error
			done = Warning("-DTLSICETransport::AddIncomingSourceGroup() media ssrc already assigned\n");
			return;
		}
		if (fec && incoming.find(fec)!=incoming.end())
		{
			//Error
			done = Warning("-DTLSICETransport::AddIncomingSourceGroup() fec ssrc already assigned\n");
			return;
		}
			
		if (rtx && incoming.find(rtx)!=incoming.end())
		{
			//Error
			done =  Warning("-DTLSICETransport::AddIncomingSourceGroup() rtx ssrc already assigned\n");
			return;
		}

		//Add rid if any
		if (!group->rid.empty())
			rids[group->mid + "@" + group->rid] = group;

		//Add mid if any
		if (!group->mid.empty())
		{
			//Find mid 
			auto it = mids.find(group->mid);
			//If not there
			if (it!=mids.end())
				//Append
				it->second.insert(group);
			else
				//Add new set
				mids[group->mid] = { group };
		}

		//Add it for each group ssrc
		if (media)
		{
			incoming[media] = group;
			recv.AddStream(media);
		}
		if (fec)
		{
			incoming[fec] = group;
			recv.AddStream(fec);
		}
		if (rtx)
		{
			incoming[rtx] = group;
			recv.AddStream(rtx);
		}
	});
	
	//Check result
	if (!done)
		return Warning("-DTLSICETransport::AddIncomingSourceGroup() Could not add incoming source\n");
		
	//If it is video and the transport wide cc is not enabled enable remb
	bool remb = group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)==RTPMap::NotFound;

	//Start distpaching
	group->Start(remb);
	
	//Done
	return true;
}

bool DTLSICETransport::RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-DTLSICETransport::RemoveIncomingSourceGroup() [mid:'%s',rid:'%s',ssrc:%u,fec:%u,rtx:%u]\n",group->mid.c_str(),group->rid.c_str(),group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//Stop distpaching
	group->Stop();
	
	//Dispatch to the event loop thread
	timeService.Sync([&](...){
		//Remove rid if any
		if (!group->rid.empty())
			rids.erase(group->mid + "@" + group->rid);

		//Remove mid if any
		if (!group->rid.empty())
		{
			//Find mid 
			auto it = mids.find(group->mid);
			//If found
			if (it!=mids.end())
			{
				//Erase group
				it->second.erase(group);
				//If it is empty now
				if(it->second.empty())
					//Remove from mids
					mids.erase(it);
			}

		}
		//Get ssrcs
		const auto media = group->media.ssrc;
		const auto rtx   = group->rtx.ssrc;
		const auto fec	 = group->fec.ssrc;

		//If got media ssrc
		if (media)
		{
			//Remove from ssrc mapping and srtp session
			incoming.erase(media);
			recv.RemoveStream(media);
		}
		//IF got fec ssrc
		if (fec)
		{
			//Remove from ssrc mapping and srtp session
			incoming.erase(fec);
			recv.RemoveStream(fec);
		}
		//IF got rtx ssrc
		if (rtx)
		{
			//Remove from ssrc mapping and srtp session
			incoming.erase(rtx);
			recv.RemoveStream(rtx);
		}
	});
	
	//Done
	return true;
}

int DTLSICETransport::Send(const RTCPCompoundPacket::shared &rtcp)
{
	//TODO: Assert event loop thread
	
	//Double check message
	if (!rtcp)
		//Log
		return Error("-DTLSICETransport::Send() | NULL rtcp message\n");
	
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
		//Log 
		return Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
	//Send buffer
	Packet buffer;
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize
	DWORD len = rtcp->Serialize(data,size);
	
	//Check result
	if (len<=0 || len>size)
	{
		//Dump it
		rtcp->Dump();
		//Error
		return Error("-DTLSICETransport::Send() | Error serializing RTCP packet [len:%d]\n",len);
	}
	
	//If we don't have an active candidate yet
	if (!active)
		//Log
		return Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");

	//If dumping
	if (dumper && dumpRTCP)
		//Write udp packet
		dumper->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);

	//Encript
	len = send.ProtectRTCP(data,len);
	
	//Check error
	if (!len)
		//Error
		return Error("-DTLSICETransport::Send() | Error protecting RTCP packet [%s]\n",send.GetLastError());

	//Store active candidate1889
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);
	//No error yet, send packet
	len = sender->Send(candidate,std::move(buffer));
	
	//Update bitrate
	outgoingBitrate.Update(getTimeMS(),len);
	
	//Return length
	return len;
}

int DTLSICETransport::SendPLI(DWORD ssrc)
{
	//Log
	Debug("-DTLSICETransport::SendPLI() | [ssrc:%u]\n",ssrc);
	
	//Execute on the event loop thread and do not wait
	timeService.Async([=](...){
		//Get group
		RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);

		//If not found
		if (!group)
			return (void)Debug("-DTLSICETransport::SendPLI() | no incoming source found for [ssrc:%u]\n",ssrc);
		
		//Get current time
		auto now = getTime();

		//Check if we have sent a PLI recently (less than half a second ago)
		if ((now-group->media.lastPLI)<1E6/2)
			//We refuse to end more
			return (void)UltraDebug("-DTLSICETransport::SendPLI() | ignored, we send a PLI recently\n");
		//Update last PLI requested time
		group->media.lastPLI = now;
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

int DTLSICETransport::Send(RTPPacket::shared&& packet)
{
	//Check packet
	if (!packet)
		//Error
		return Error("-DTLSICETransport::Send() | Error null packet\n");
	
	//Check if we have an active DTLS connection yet
	if (!send.IsSetup())
		//Error
		return Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
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
	
        //SYNCHRONIZED
        {
                //Block scope
		ScopedLock scope(source);
                //Update headers
                packet->SetExtSeqNum(source.CorrectExtSeqNum(packet->GetExtSeqNum()));
                packet->SetSSRC(source.ssrc);
        }
        
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
	
	//if (group->type==MediaFrame::Video) UltraDebug("-Sending RTP on media:%s sssrc:%u seq:%u pt:%u ts:%lu codec:%s\n",MediaFrame::TypeToString(group->type),source.ssrc,packet->GetSeqNum(),packet->GetPayloadType(),packet->GetTimestamp(),GetNameForCodec(group->type,packet->GetCodec()));
	
	//Send buffer
	Packet buffer;
	BYTE* 	data = buffer.GetData();
	DWORD	size = buffer.GetCapacity();
	
	//Serialize data
	int len = packet->Serialize(data,size,sendMaps.ext);
	
	//IF failed
	if (!len)
		return Warning("-DTLSICETransport::Send() | Could not serialize packet\n");

	//Add packet for RTX
	group->AddPacket(packet);
	
	//If we don't have an active candidate yet
	if (!active)
		//Error
		return Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");

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
		//Error
		return Error("-RTPTransport::SendPacket() | Error protecting RTP packet [ssrc:%u,%s]\n",ssrc,send.GetLastError());

	//Store candidate
	ICERemoteCandidate* candidate = active;

	//Set buffer size
	buffer.SetSize(len);
	//No error yet, send packet
	sender->Send(candidate,std::move(buffer));
	//Get time
	now = getTime();
	//Update bitrate
	outgoingBitrate.Update(now/1000,len);
	
	DWORD bitrate   = 0;
	DWORD estimated = 0;
	DWORD probing	= 0;
		
	//SYNCHRONIZED
	{
		//Block scope
		ScopedLock scope(source);
		//Update last items
		source.lastTime		= packet->GetTimestamp();
		source.lastPayloadType  = packet->GetPayloadType();
		//Set clockrate
		source.clockrate	= packet->GetClockRate();
		
		//Update source
		source.Update(now/1000,packet->GetSeqNum(),len);
		
		 //Get bitrates
		bitrate   = static_cast<DWORD>(source.acumulator.GetInstantAvg()*8);
		estimated = source.remb;
		probing	  = static_cast<DWORD>(probingBitrate.GetInstantAvg()*8);
	}

	//Check if we are using transport wide for this packet
	if (packet->HasTransportWideCC())
	{
		//Create stats
		auto stats = PacketStats::Create(packet,len,now);
		//Add new stat
		senderSideBandwidthEstimator.SentPacket(stats);
	}

	//Get time for packets to discard, always have at least 200ms, max 500ms
	QWORD until = now/1000 - (200+fmin(rtt*2,300));
	
	//Release packets from rtx queue
	group->ReleasePackets(until);
	
	//If packet is an key frame
	if (packet->IsKeyFrame())
		//Do not retransmit packets before this frame
		group->ReleasePacketsByTimestamp(packet->GetExtTimestamp());
	
	//Check if we need to send SR (1 per second)
	if (now-source.lastSenderReport>1E6)
		//Create and send rtcp sender retpor
		Send(RTCPCompoundPacket::Create(group->media.CreateSenderReport(now)));
	
	//Check if this packets support rtx
	bool rtx = group->rtx.ssrc && !sendMaps.apt.empty();
	
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
	{
		//Append it to the end of the packet history
		history.push_back(packet);
		//Remove old ones
		while (history.size()>10)
			//Remove first
			history.pop_front();
	}
	
	//Did we send successfully?
	return (len>=0);
}

void DTLSICETransport::onRTCP(const RTCPCompoundPacket::shared& rtcp)
{
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
								//Lock source
								ScopedLock scoped(*source);
								//Process report
								if (source->ProcessReceiverReport(now/1000, report))
									//We need to update rtt
									SetRTT(source->rtt);
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
								//Lock source
								ScopedLock scoped(*source);
								//Process report
								if (source->ProcessReceiverReport(now/1000, report))
									//We need to update rtt
									SetRTT(source->rtt);
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
				//Get media
				RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
				//If not found
				if (!group)
				{
					//Dump
					fb->Dump();
					//Debug
					Warning("-DTLSICETransport::onRTCP() | Got feedback message for unknown media  [ssrc:%u]\n",ssrc);
					//Ups! Skip
					continue;
				}
				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPRTPFeedback::NACK:
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							auto field = fb->GetField<RTCPRTPFeedback::NACKField>(i);
							
							//Resent it
							ReSendPacket(group,field->pid);
							//Check each bit of the mask
							for (BYTE i=0;i<16;i++)
								//Check it bit is present to rtx the packets
								if ((field->blp >> i) & 1)
									//Resent it
									ReSendPacket(group,field->pid+i+1);
						}
						break;
					case RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest:
						UltraDebug("-DTLSICETransport::onRTCP() | TempMaxMediaStreamBitrateRequest\n");
						break;
					case RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification:
						UltraDebug("-DTLSICETransport::onRTCP() | TempMaxMediaStreamBitrateNotification\n");
						break;
					case RTCPRTPFeedback::TransportWideFeedbackMessage:
						//Get each fiedl
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							auto field = fb->GetField<RTCPRTPFeedback::TransportWideFeedbackMessageField>(i);
							//Pass it to the estimator
							senderSideBandwidthEstimator.ReceivedFeedback(field->feedbackPacketCount,field->packets,now);
						}
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
				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPPayloadFeedback::PictureLossIndication:
					case RTCPPayloadFeedback::FullIntraRequest:
					{
						//Get media
						RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
						
						//Dbug
						Debug("-DTLSICETransport::onRTCP() | FPU requested [ssrc:%u,group:%p,this:%p]\n",ssrc,group,this);
						
						//If not found
						if (!group)
						{
							//Dump
							fb->Dump();
							//Debug
							Error("-Got feedback message for unknown media  [ssrc:%u]\n",ssrc);
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
						for (BYTE i=0;i<fb->GetFieldCount();i++)
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
									
									//If found
									if (group)
										//Call listener
										group->onREMB(target,bitrate);
								}
							}
						}
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

void DTLSICETransport::SetRTT(DWORD rtt)
{
	//Debug
	UltraDebug("-DTLSICETransport::SetRTT() [rtt:%d]\n",rtt);
	//Sore it
	this->rtt = rtt;
	//Update jitters
	for (auto it : incoming)
		//Update jitter
		it.second->SetRTT(rtt);
	//Add estimation
	senderSideBandwidthEstimator.UpdateRTT(getTime(),rtt);
}

void DTLSICETransport::SendTransportWideFeedbackMessage(DWORD ssrc)
{
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
		auto stats = it->second;
		//Get transport seq
		DWORD transportExtSeqNum = it->first;
		//Get relative time
		QWORD time = stats->time - initTime;

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
	Debug("-DTLSICETransport::Start()\n");
	
	//Get init time
	initTime = getTime();
	dcOptions.localPort = 5000;
	dcOptions.remotePort = 5000;
	//Run ice timeout timer
	iceTimeoutTimer = timeService.CreateTimer(IceTimeout,[this](...){
		//If got listener
		if (listener)
			//Fire timeout 
			listener->onICETimeout();
	});
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
	Debug(">DTLSICETransport::Stop()\n");
	
	//Check probing timer
	if (probingTimer)
	{
		//Stop probing
		probingTimer->Cancel();
		//Remove timer
		probingTimer.reset();
	}
	
	//Check ice timeout timer
	if (iceTimeoutTimer)
		//Stop probing
		iceTimeoutTimer->Cancel();

	//Stop
	endpoint.Close();
	
	//Not started anymore
	started = false;
	
	//Log
	Debug("<DTLSICETransport::Stop()\n");
}

int DTLSICETransport::Enqueue(const RTPPacket::shared& packet)
{
	//Send async
	timeService.Async([this,packet](...){
		//Send
		Send(packet->Clone());
	});
	
	return 1;
}

int DTLSICETransport::Enqueue(const RTPPacket::shared& packet,std::function<RTPPacket::shared(const RTPPacket::shared&)> modifier)
{
	//Send async
	timeService.Async([this,packet,modifier](...){
		//Send
		Send(modifier(packet));
	});
	
	return 1;
}
void DTLSICETransport::Probe()
{
	//Endure that transport wide cc is enabled
	if (probe && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
	{
		
		//Get sleep time
		uint64_t now = getTime();
		//Update bitrates
		outgoingBitrate.Update(now/1000);
		probingBitrate.Update(now/1000);
		//Calculate sleep time
		uint64_t sleep = lastProbe ? (now - lastProbe)/1000 : probingTimer->GetRepeat().count();
		//Update last probe time
		lastProbe = now;
		//Get bitrates
		DWORD bitrate		= static_cast<DWORD>(outgoingBitrate.GetInstantAvg()*8);
		DWORD probing		= static_cast<DWORD>(probingBitrate.GetInstantAvg()*8);
		DWORD target		= senderSideBandwidthEstimator.GetTargetBitrate();

		//Log(">DTLSICETransport::Probe() | [target:%u,bitrate:%d,probing:%d,history:%d]\n",target,bitrate,probing,history.size());
			
		//If we can still send more
		if (target>bitrate && (!probingBitrateLimit || bitrate<probingBitrateLimit) && probing<maxProbingBitrate)
		{
			//Increase probing bitrate
			probing += std::min(maxProbingBitrate, target - bitrate);

			//Get size of probes
			DWORD probingSize = (probing*sleep)/8000;
			
			//Log("-DTLSICETransport::Probe() | Sending probing packets [target:%u,bitrate:%u,probing:%u,max:%u,probingSize:%d,sleep:%d]\n", target, bitrate,probing,maxProbingBitrate, probingSize, sleep);

			//If we have packet history
			if (history.size())
			{
				//Sent until no more probe
				while (probingSize)
				{
					//For each packet in history
					for (auto it = history.rbegin(); it!=history.rend(); ++it)
					{
						//Send probe packet
						DWORD len = SendProbe(*it);
						//Check len
						if (!len)
							//Done
							return;
						//Update probing
						probingBitrate.Update(now/1000,len);
						//Check size
						if (len>probingSize)
							//Done
							return;
						//Reduce probe
						probingSize -= len;
						//Update now
						now = getTime();
					}
					
				}
			} else {
				//Ensure we send at least one packet
				DWORD size = std::min(255u,probingSize);
				//Check if we have an outgpoing group
				for (auto &group : outgoing)
				{
					//We can only probe on rtx with video
					if (group.second->type == MediaFrame::Video)
					{
						//Set all the probes
						while (probingSize>=size)
						{
							//Send probe packet
							DWORD len = SendProbe(group.second,size);
							//Check len
							if (!len)
								//Done
								return;
							//Update probing
							probingBitrate.Update(now/1000,len);
							//Check size
							if (len>probingSize)
								//Done
								return;
							//Reduce probe
							probingSize -= len;
							//Update now
							now = getTime();
						}
						//Done
						return;
					}
				}
			}
		}
	}
}

void DTLSICETransport::SetBandwidthProbing(bool probe)
{
	//Set probing status
	this->probe = probe;
}

void DTLSICETransport::SetListener(Listener* listener)
{
	//Add in main thread and wait
	timeService.Sync([=](...){
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
