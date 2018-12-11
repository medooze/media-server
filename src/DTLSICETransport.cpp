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
#include "rtpsession.h"
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

DTLSICETransport::DTLSICETransport(Sender *sender) :
	dtls(*this),
	mutex(true),
	incomingBitrate(1000),
	outgoingBitrate(1000)
{
	//Store sender
	this->sender = sender;
	//Add dummy stream to stimator
	senderSideEstimator.AddStream(0);
}

DTLSICETransport::~DTLSICETransport()
{
	//Reset & stop
	Reset();
	Stop();
}

int DTLSICETransport::onData(const ICERemoteCandidate* candidate,BYTE* data,DWORD size)
{
	RTPHeader header;
	RTPHeaderExtension extension;
	DWORD len = size;
	
	
	//Synchronized
	{
		//Lock
		ScopedLock lock(mutex);
		//Acumulate bitrate
		incomingBitrate.Update(getTimeMS(),size);
	}
	
	//Check if it a DTLS packet
	if (DTLSConnection::IsDTLS(data,size))
	{
		//Feed it
		dtls.Write(data,size);

		//Read buffers are always MTU size
		DWORD len = dtls.Read(data,MTU);
		
		//Synchronized
		{
			//Lock
			ScopedLock lock(mutex);
			//Acumulate bitrate
			outgoingBitrate.Update(getTimeMS(),len);
		}

		//Check it
		if (len>0)
			//Send it back
			sender->Send(candidate,data,len);
		//Exit
		return 1;
	}

	//Check if it is RTCP
	if (RTCPCompoundPacket::IsRTCP(data,size))
	{

		//Check session
		if (!recv)
			return Error("-DTLSICETransport::onData() | No recvSRTPSession\n");

		//unprotect
		srtp_err_status_t err = srtp_unprotect_rtcp(recv,data,(int*)&len);
		//Check error
		if (err!=srtp_err_status_ok)
			return Error("-DTLSICETransport::onData() | Error unprotecting rtcp packet [%d]\n",err);

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
	if (!recv)
		return Error("-DTLSICETransport::onData() | No recvSRTPSession\n");
	//unprotect
	srtp_err_status_t err = srtp_unprotect(recv,data,(int*)&len);
	//Check status
	if (err!=srtp_err_status_ok)
	{
		//Error
		Error("-DTLSICETransport::onData() | Error unprotecting rtp packet [%d]\n",err);
		//Parse RTP header
		DWORD ini = header.Parse(data,size);
		//If it has extension
		if (header.extension)
		{
			//Parse extension
			extension.Parse(recvMaps.ext,data+ini,size-ini);
			extension.Dump();
		}
		//Error
		return 0;
	}
	
	//If dumping
	if (dumper && dumpInRTP)
		//Write udp packet
		dumper->WriteUDP(getTimeMS(),candidate->GetIPAddress(),candidate->GetPort(),0x7F000001,5004,data,len);
	
	//Parse RTP header
	DWORD ini = header.Parse(data,size);
	
	//On error
	if (!ini)
	{
		//Debug
		Debug("-DTLSICETransport::onData() | Could not parse RTP header\n");
		//Dump it
		::Dump(data,size);
		//Exit
		return 1;
	}
	
	//If it has extension
	if (header.extension)
	{
		//Parse extension
		DWORD l = extension.Parse(recvMaps.ext,data+ini,size-ini);
		//If not parsed
		if (!l)
		{
			///Debug
			Debug("-DTLSICETransport::onData() | Could not parse RTP header extension\n");
			//Dump it
			::Dump(data,size);
			//Exit
			return 1;
		}
		//Inc ini
		ini += l;
	}

	//Check size with padding
	if (header.padding)
	{
		//Get last 2 bytes
		WORD padding = get1(data,len-1);
		//Ensure we have enought size
		if (len-ini<padding)
		{
			///Debug
			Debug("-DTLSICETransport::onData() | RTP padding is bigger than size [padding:%u,size%u]\n",padding,len);
			//Exit
			return 0;
		}
		//Remove from size
		len -= padding;
	}
	
	//Get ssrc
	DWORD ssrc = header.ssrc;
	
	//Get initial codec
	BYTE codec = recvMaps.rtp.GetCodecForType(header.payloadType);
	
	//Check codec
	if (codec==RTPMap::NotFound)
		//Exit
		return Error("-DTLSICETransport::onData() | RTP packet payload type unknown [%d]\n",header.payloadType);
	//Get media
	MediaFrame::Type media = GetMediaForCodec(codec);
	
	//Create normal packet
	auto packet = std::make_shared<RTPPacket>(media,codec,header,extension);
	
	//Set the payload
	packet->SetPayload(data+ini,len-ini);
	
	//Using incoming
	ScopedUse use(incomingUse);
	
	//Get group
	RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);
	
	//If it doesn't have a group
	if (!group)
	{
		//Get rid
		auto mid = extension.mid;
		auto rid = extension.hasRepairedId ? extension.repairedId : extension.rid;
			
		Debug("-DTLSICETransport::onData() | Unknowing group for ssrc trying to retrieve by [ssrc:%u,rid:'%s']\n",ssrc,extension.rid.c_str());
		
		//If it is the repaidr stream or it has rid and it is rtx
		if (extension.hasRepairedId || (extension.hasRId && packet->GetCodec()==VideoCodec::RTX))
		{
			//Try to find it on the rids and mids
			auto it = rids.find(mid+"@"+rid);
			//If found
			if (it!=rids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtx stream to ssrc [ssrc:%u,mid:'%s',rid:'%s']\n",ssrc,mid.c_str(),rid.c_str());

				//Got source
				group = it->second;

				//Set ssrc for next ones
				//TODO: ensure it didn't had a previous ssrc
				group->rtx.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
			}
		} else if (extension.hasRId) {
			//Try to find it on the rids and mids
			auto it = rids.find(mid+"@"+rid);
			//If found
			if (it!=rids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtp stream to ssrc [ssrc:%u,mid:'%s',rid:'%s']\n",ssrc,mid.c_str(),rid.c_str());

				//Got source
				group = it->second;

				//Set ssrc for next ones
				//TODO: ensure it didn't had a previous ssrc
				group->media.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
			}
		} else if (extension.hasMediaStreamId && packet->GetCodec()==VideoCodec::RTX) {
			//Try to find it on the rids and mids
			auto it = mids.find(mid);
			//If found
			if (it!=mids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtx stream id to ssrc [ssrc:%u,mid:'%s']\n",ssrc,mid.c_str());

				//Get first source in set, if there was more it should have contained an rid
				group = *it->second.begin();
				
				//Set ssrc for next ones
				//TODO: ensure it didn't had a previous ssrc
				group->rtx.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
			}
		} else if (extension.hasMediaStreamId) {
			//Try to find it on the rids and mids
			auto it = mids.find(mid);
			//If found
			if (it!=mids.end())
			{
				Log("-DTLSICETransport::onData() | Associating rtp stream to ssrc [ssrc:%u,mid:'%s']\n",ssrc,mid.c_str());

				//Get first source in set, if there was more it should have contained an rid
				group = *it->second.begin();

				//Set ssrc for next ones
				//TODO: ensure it didn't had a previous ssrc
				group->media.ssrc = ssrc;

				//Add it to the incoming list
				incoming[ssrc] = group;
			}
		}
		
	}
			
	//Ensure it has a group
	if (!group)	
		//error
		return Error("-DTLSICETransport::onData() | Unknowing group for ssrc [%u]\n",ssrc);
	
	//Assing mid if found
	if (group->mid.empty() && extension.hasMediaStreamId)
	{
		//Debug
		Debug("-DTLSICETransport::onData() | Assinging media stream id [ssrc:%u,mid:'%s']\n",ssrc,extension.mid.c_str());
		//Set it
		group->mid = extension.mid;
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
	
	//UltraDebug("-Got RTP on media:%s sssrc:%u seq:%u pt:%u codec:%s rid:'%s', mid:'%s'\n",MediaFrame::TypeToString(group->type),ssrc,header.sequenceNumber,header.payloadType,GetNameForCodec(group->type,codec),group->rid.c_str(),group->mid.c_str());
	
	//Process packet and get source
	RTPIncomingSource* source = group->Process(packet);
	
	//Ensure it has a source
	if (!source)
		//error
		return Error("-DTLSICETransport::onData() | Group does not contain ssrc [%u]\n",ssrc);

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
		
		//Add packets to the transport wide stats
		transportWideReceivedPacketsStats[transportExtSeqNum] = PacketStats::Create(packet,size,getTime());
		
		//If we have enought or timeout (500ms)
		//TODO: Implement timer
		if (transportWideReceivedPacketsStats.size()>20 || (getTime()-transportWideReceivedPacketsStats.begin()->second->time)>5E5)
			//Send feedback message
			SendTransportWideFeedbackMessage(ssrc);
	}

	//If it was an RTX packet and not a padding only one
	if (ssrc==group->rtx.ssrc && packet->GetMediaLength()) 
	{
		//Ensure that it is a RTX codec
		if (codec!=VideoCodec::RTX)
			//error
			return  Error("-DTLSICETransport::onData() | No RTX codec on rtx sssrc:%u type:%d codec:%d\n",packet->GetSSRC(),packet->GetPayloadType(),packet->GetCodec());
	
		//Find apt type
		auto apt = recvMaps.apt.GetCodecForType(packet->GetPayloadType());
		//Find codec 
		codec = recvMaps.rtp.GetCodecForType(apt);
		//Check codec
		 if (codec==RTPMap::NotFound)
			  //Error
			  return Error("-DTLSICETransport::onData() | RTP RTX packet apt type unknown [%d]\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType());
		
		//Remove OSN and restore seq num
		if (!packet->RecoverOSN())
			//Error
			return Error("-DTLSICETransport::onData() | RTP Could not recoever OSX\n");
		
		 //Set original ssrc
		 packet->SetSSRC(group->media.ssrc);
		 //Set corrected seq num cycles
		 packet->SetSeqCycles(group->media.RecoverSeqNum(packet->GetSeqNum()));
		 //Set codec
		 packet->SetCodec(codec);
		 packet->SetPayloadType(apt);

	} else if (ssrc==group->fec.ssrc)  {
		UltraDebug("-Flex fec\n");
		//Ensure that it is a FEC codec
		if (codec!=VideoCodec::FLEXFEC)
			//error
			return  Error("-DTLSICETransport::onData() | No FLEXFEC codec on fec sssrc:%u type:%d codec:%d\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType(),packet->GetSSRC());
		//DO NOTHING with it yet
		return 1;
	}	

	//Add packet and see if we have lost any in between
	int lost = group->AddPacket(packet,size);

	//Check if it was rejected
	if (lost<0)
	{
		UltraDebug("-DTLSICETransport::onData() | Dropped packet [orig:%u,ssrc:%u,seq:%d,rtx:%d]\n",ssrc,packet->GetSSRC(),packet->GetSeqNum(),ssrc==group->rtx.ssrc);
		//Increase rejected counter
		source->dropPackets++;
	} 
	
	if ( group->type == MediaFrame::Video && 
		( lost>0 ||  (group->GetCurrentLost() && getTimeDiff(source->lastNACKed)/1000>fmax(rtt,20)))
	   )
	{
		UltraDebug("-DTLSICETransport::onData() | Lost packets [ssrc:%u,ssrc:%u,seq:%d,lost:%d,total:%d]\n",ssrc,packet->GetSSRC(),packet->GetSeqNum(),lost,group->GetCurrentLost());

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
		source->lastNACKed = getTime();
		//Update nacked packets
		source->totalNACKs++;
	}
	
	//Check if we need to send RR (1 per second)
	if (getTimeDiff(source->lastReport)>1E6)
	{
		//Create rtcp sender retpor
		auto rtcp = RTCPCompoundPacket::Create();
		
		//Create receiver report for normal stream
		auto rr = rtcp->CreatePacket<RTCPReceiverReport>(mainSSRC);
		
		//Create report
		auto report = source->CreateReport(getTime());

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
			Log("-REMB [ssrc:%x,mid:'%s',count:%d,bitrate:%u]\n",group->media.ssrc,group->mid.c_str(),ssrcs.size(),bitrate);
			
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
			WORD last = group->SetRTTRTX(getTime());
			//Request it
			nack->AddField(std::make_shared<RTCPRTPFeedback::NACKField>(last,0));
		}
	
		//Send it
		Send(rtcp);
	}
	
	//Done
	return 1;
}

void DTLSICETransport::SendProbe(RTPOutgoingSourceGroup *group,BYTE padding)
{
	//Check if we have an active DTLS connection yet
	if (!send)
		//Error
		return (void) Debug("-DTLSICETransport::SendProbe() | We don't have an DTLS setup yet\n");
	
	//Data
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	DWORD size = MTU;
	int len = 0;

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
		extension.absSentTime = getTimeMS();
	}

	//Serialize header
	int n = header.Serialize(data,size);

	//Comprobamos que quepan
	if (!n)
		//Error
		return (void)Error("-DTLSICETransport::ReSendPacket() | Error serializing rtp headers\n");

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
			return (void)Error("-DTLSICETransport::ReSendPacket() | Error serializing rtp extension headers\n");
		//Inc len
		len += n;
	}

	//Set 0 padding
	memset(data+len,0,padding);
	
	//Set pateckt length
	len += padding;
	
	//Set padding size
	data[len++] = padding;

	ICERemoteCandidate* candidate;
	//Synchronized
	{
		//Block scope
		ScopedLock method(mutex);

		//If we don't have an active candidate yet
		if (!active)
			//Error
			return (void)Debug("-DTLSICETransport::SendProbe() | We don't have an active candidate yet\n");
		
		//If dumping
		if (dumper && dumpOutRTP)
			//Write udp packet
			dumper->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);

		
		//Encript
		srtp_err_status_t err = srtp_protect(send,data,(int*)&len);
		//Check error
		if (err!=srtp_err_status_ok)
			//Error
			return (void)Error("-RTPTransport::SendProbe() | Error protecting RTP packet [%d]\n",err);

		//Store candidate before unlocking
		candidate = active;
		
		//Update bitrate
		outgoingBitrate.Update(getTimeMS(),len);
	}

	//No error yet, send packet
	if (sender->Send(candidate,data,len)<=0)
		//Error
		Error("-RTPTransport::SendPacket() | Error sending RTP packet [%d]\n",errno);
	
        //SYNC
        {
                //Lock in scope
                ScopedLock scope(source);
                //Update last send time
                source.lastTime		= header.timestamp;
                source.lastPayloadType  = header.payloadType;
	
                //Update stats
                source.Update(getTimeMS(),header.sequenceNumber,len);
        }
	
	//Add to transport wide stats
	if (extension.hasTransportWideCC)
	{
		//Block method
		ScopedLock method(transportWideMutex);
		//Create stat
		auto stats = std::make_shared<PacketStats>();
		//Fill
		stats->transportWideSeqNum	= extension.transportSeqNum;
		stats->ssrc			= header.ssrc;
		stats->extSeqNum		= extSeqNum;
		stats->size			= len;
		stats->payload			= 0;
		stats->timestamp		= header.timestamp;
		stats->time			= getTime();
		stats->mark			= false;
		//Add new stat
		transportWideSentPacketsStats[extension.transportSeqNum] = stats;
		//Protect against missing feedbacks, remove too old lost packets
		auto it = transportWideSentPacketsStats.begin();
		//If we have more than 1s diff
		while(it!=transportWideSentPacketsStats.end() && getTimeDiff(it->second->time)>1E6)
			//Erase it and move iterator
			it = transportWideSentPacketsStats.erase(it);
	}
}

void DTLSICETransport::ReSendPacket(RTPOutgoingSourceGroup *group,WORD seq)
{
	
	//Check if we have an active DTLS connection yet
	if (!send)
	{
		//Error
		Debug("-DTLSICETransport::ReSendPacket() | We don't have an DTLS setup yet\n");
		return;
	}
	
	UltraDebug("-DTLSICETransport::ReSendPacket() | resending [seq:%d,ssrc:%u]\n",seq,group->rtx.ssrc);
	
	//Find packet to retransmit
	auto packet = group->GetPacket(seq);

	//If we don't have it anymore
	if (!packet)
	{
		//Debug
		UltraDebug("-DTLSICETransport::ReSendPacket() | packet not found[seq:%d,ssrc:%u]\n",seq,group->rtx.ssrc);
		return;
	}

	//Data
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	DWORD size = MTU;
	int len = 0;

	//Overrride headers
	RTPHeader		header(packet->GetRTPHeader());
	RTPHeaderExtension	extension(packet->GetRTPHeaderExtension());

	//Try to send it via rtx
	BYTE apt = sendMaps.apt.GetTypeForCodec(packet->GetPayloadType());
		
	//Check if we ar using rtx or not
	bool rtx = group->rtx.ssrc && apt!=RTPMap::NotFound;
		
	//Check which source are we using
	RTPOutgoingSource& source = rtx ? group->rtx : group->media;
	
	//Get extended sequence number
	DWORD extSeqNum = packet->GetExtSeqNum();
	
	//If it is using rtx (i.e. not firefox)
	if (rtx)
	{
                //Lock in scope
                ScopedLock scope(source);
		//Update RTX headers
		header.ssrc		= source.ssrc;
		header.payloadType	= apt;
		header.sequenceNumber	= extSeqNum = source.NextSeqNum();
		//No padding
		header.padding		= 0;
	}

	//Add transport wide cc on video
	if (group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC!=RTPMap::NotFound))
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
		extension.absSentTime = getTimeMS();
	}

	//Serialize header
	int n = header.Serialize(data,size);

	//Comprobamos que quepan
	if (!n)
		//Error
		return (void)Error("-DTLSICETransport::ReSendPacket() | Error serializing rtp headers\n");

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
			return (void)Error("-DTLSICETransport::ReSendPacket() | Error serializing rtp extension headers\n");
		//Inc len
		len += n;
	}

	//If it is using rtx (i.e. not firefox)
	if (rtx)
	{
		//And set the original seq
		set2(data,len,seq);
		//Move payload start
		len += 2;
	}

	//Ensure we have enougth data
	if (len+packet->GetMediaLength()>size)
		//Error
		return (void)Error("-DTLSICETransport::ReSendPacket() | Media overflow\n");

	//Copiamos los datos
	memcpy(data+len,packet->GetMediaData(),packet->GetMediaLength());

	//Set pateckt length
	len += packet->GetMediaLength();

	ICERemoteCandidate* candidate;
	//Synchronized
	{
		//Block scope
		ScopedLock method(mutex);

		//If we don't have an active candidate yet
		if (!active)
			//Error
			return (void)Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");

		//If dumping
		if (dumper && dumpOutRTP)
			//Write udp packet
			dumper->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);

		//Encript
		srtp_err_status_t err = srtp_protect(send,data,(int*)&len);
		//Check error
		if (err!=srtp_err_status_ok)
			//Error
			return (void)Error("-RTPTransport::SendPacket() | Error protecting RTP packet [%d]\n",err);

		//Store candidate before unlocking
		candidate = active;
		
		//Update bitrate
		outgoingBitrate.Update(getTimeMS(),len);
	}

	//No error yet, send packet
	if (sender->Send(candidate,data,len)<=0)
		//Error
		Error("-RTPTransport::SendPacket() | Error sending RTP packet [%d]\n",errno);
	
        //Synchronized
	{
		//Block scope
		ScopedLock scope(source);
                //Update last send time
                source.lastTime		= packet->GetTimestamp();
                source.lastPayloadType  = packet->GetPayloadType();
	
                //Update stats
                source.Update(getTimeMS(),header.sequenceNumber,len);
        }
	
	//Add to transport wide stats
	if (extension.hasTransportWideCC)
	{
		//Block method
		ScopedLock method(transportWideMutex);
		//Create stat
		auto stats = std::make_shared<PacketStats>();
		//Fill
		stats->transportWideSeqNum	= extension.transportSeqNum;
		stats->ssrc			= header.ssrc;
		stats->extSeqNum		= extSeqNum;
		stats->size			= len;
		stats->payload			= packet->GetMediaLength();
		stats->timestamp		= header.timestamp;
		stats->time			= getTime();
		stats->mark			= false;
		//Add new stat
		transportWideSentPacketsStats[extension.transportSeqNum] = stats;
		//Protect against missing feedbacks, remove too old lost packets
		auto it = transportWideSentPacketsStats.begin();
		//If we have more than 1s diff
		while(it!=transportWideSentPacketsStats.end() && getTimeDiff(it->second->time)>1E6)
			//Erase it and move iterator
			it = transportWideSentPacketsStats.erase(it);
	}
}

void  DTLSICETransport::ActivateRemoteCandidate(ICERemoteCandidate* candidate,bool useCandidate, DWORD priority)
{
	//Lock
	mutex.Lock();
	
	//Debug
	//UltraDebug("-DTLSICETransport::ActivateRemoteCandidate() | Remote candidate [%s:%hu,use:%d,prio:%d]\n",candidate->GetIP(),candidate->GetPort(),useCandidate,priority);
	
	//Should we set this candidate as the active one
	if (!active || (useCandidate && candidate!=active))
	{
		//Debug
		Debug("-DTLSICETransport::ActivateRemoteCandidate() | Activating candidate [%s:%hu,use:%d,prio:%d,dtls:%d]\n",candidate->GetIP(),candidate->GetPort(),useCandidate,priority,dtls.GetSetup());	
		
		//Send data to this one from now on
		active = candidate;
	}
	
	//Unlock
	mutex.Unlock();
	
	// Needed for DTLS in client mode (otherwise the DTLS "Client Hello" is not sent over the wire)
	if (dtls.GetSetup()!=DTLSConnection::SETUP_PASSIVE) 
	{
		BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
		DWORD len = dtls.Read(data,MTU);
		//Check it
		if (len>0)
			//Send to bundle transport
			sender->Send(active,data,len);
		
		//Lock
		ScopedLock lock(mutex);
		//Update bitrate
		outgoingBitrate.Update(getTimeMS(),len);
	}
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
int DTLSICETransport::Dump(UDPDumper* dumper, bool inbound, bool outbound, bool rtcp)
{
	Debug("-DTLSICETransport::Dump() | [inbound:%d,outboud:%d,rtcp:%d]\n",inbound,outbound,rtcp);
	
	if (this->dumper)
		return Error("Already dumping\n");
	
	//Store pcap as dumper
	this->dumper = dumper;
	
	//What to dumo
	dumpInRTP	= inbound;
	dumpOutRTP	= outbound;
	dumpRTCP	= rtcp;
	
	//Done
	return 1;
}

int DTLSICETransport::Dump(const char* filename, bool inbound, bool outbound, bool rtcp)
{
	Log("-DTLSICETransport::Dump() | [pcap:%s]\n",filename);
	
	if (dumper)
		return Error("Already dumping\n");
	
	//Create dumper file
	PCAPFile* pcap = new PCAPFile();
	
	//Open it
	if (!pcap->Open(filename))
	{
		//Error
		delete(pcap);
		return 0;
	}
	
	//Ok
	return Dump(pcap,inbound,outbound,rtcp);
}

void DTLSICETransport::Reset()
{
	Log("-DTLSICETransport::Reset()\n");

	//Clean mem
	if (iceLocalUsername)
		free(iceLocalUsername);
	if (iceLocalPwd)
		free(iceLocalPwd);
	if (iceRemoteUsername)
		free(iceRemoteUsername);
	if (iceRemotePwd)
		free(iceRemotePwd);
	//If secure
	if (send)
		//Dealoacate
		srtp_dealloc(send);
	//If secure
	if (recv)
		//Dealoacate
		srtp_dealloc(recv);
	
	//Check if dumping
	if (dumper)
	{
		//Close dumper
		dumper->Close();
		//Delete it
		delete(dumper);
	}
	
	send = NULL;
	recv = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	dumper = NULL;
	dumpInRTP = false;
	dumpOutRTP = false;
	dumpRTCP = false;
}

int DTLSICETransport::SetLocalCryptoSDES(const char* suite,const BYTE* key,const DWORD len)
{
	srtp_err_status_t err;
	srtp_policy_t policy;

	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	//Get cypher
	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		Log("-DTLSICETransport::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-DTLSICETransport::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-DTLSICETransport::SetLocalCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-DTLSICETransport::SetLocalCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
#ifdef SRTP_GCM
	} else if (strcmp(suite,"AEAD_AES_256_GCM")==0) {
		Log("-DTLSICETransport::SetLocalCryptoSDES() | suite: AEAD_AES_256_GCM\n");
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_128_GCM")==0) {
		Log("-DTLSICETransport::SetLocalCryptoSDES() | suite: AEAD_AES_128_GCM\n");
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
#endif
	} else {
		return Error("-DTLSICETransport::SetLocalCryptoSDES() | Unknown cipher suite: %s", suite);
	}

	//Check sizes
	if (len!=(DWORD)policy.rtp.cipher_key_len)
		//Error
		return Error("-DTLSICETransport::SetLocalCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

	//Set polciy values
	policy.ssrc.type	= ssrc_any_outbound;
	policy.ssrc.value	= 0;
	policy.allow_repeat_tx  = 1;
	policy.window_size	= 1024;
	policy.key		= (BYTE*)key;
	policy.next		= NULL;
	//Create new
	srtp_t session;
	err = srtp_create(&session,&policy);

	//Check error
	if (err!=srtp_err_status_ok)
		//Error
		return Error("-DTLSICETransport::SetLocalCryptoSDES() | Failed to create local SRTP session | err:%d\n", err);
	
	//if we already got a send session don't leak it
	if (send)
		//Dealoacate
		srtp_dealloc(send);

	//Set send SSRTP sesion
	send = session;

	//Evrything ok
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

	//Init DTLS
	return dtls.Init();
}

int DTLSICETransport::SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len)
{
	srtp_err_status_t err;
	srtp_policy_t policy;

	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		Log("-DTLSICETransport::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-DTLSICETransport::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-DTLSICETransport::SetRemoteCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-DTLSICETransport::SetRemoteCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
#ifdef SRTP_GCM
	} else if (strcmp(suite,"AEAD_AES_256_GCM")==0) {
		Log("-DTLSICETransport::SetRemoteCryptoSDES() | suite: AEAD_AES_256_GCM\n");
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_128_GCM")==0) {
		Log("-DTLSICETransport::SetRemoteCryptoSDES() | suite: AEAD_AES_128_GCM\n");
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
#endif
	} else {
		return Error("-DTLSICETransport::SetRemoteCryptoSDES() | Unknown cipher suite %s", suite);
	}

	//Check sizes
	if (len!=(DWORD)policy.rtp.cipher_key_len)
		//Error
		return Error("-DTLSICETransport::SetRemoteCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

	//Set polciy values
	policy.ssrc.type	= ssrc_any_inbound;
	policy.ssrc.value	= 0;
	policy.key		= (BYTE*)key;
	policy.next		= NULL;

	//Create new
	srtp_t session;
	err = srtp_create(&session,&policy);

	//Check error
	if (err!=srtp_err_status_ok)
		//Error
		return Error("-DTLSICETransport::SetRemoteCryptoSDES() | Failed to create remote SRTP session | err:%d\n", err);
	
	//if we already got a recv session don't leak it
	if (recv)
		//Dealoacate
		srtp_dealloc(recv);
	//Set it
	recv = session;

	//Everything ok
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
			Error("-DTLSICETransport::onDTLSSetup() | Unknown suite\n");
	}
}

bool DTLSICETransport::AddOutgoingSourceGroup(RTPOutgoingSourceGroup *group)
{
	//Log
	Log("-AddOutgoingSourceGroup [group:%p,ssrc:%u,fec:%u,rtx:%u]\n",group,group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//Scoped sync
	{
		//Using outgoing
		ScopedUse use(outgoingUse);
		
		//Check they are not already assigned
		if (outgoing.find(group->media.ssrc)!=outgoing.end())
			return Error("-AddOutgoingSourceGroup media ssrc already assigned");
		if (outgoing.find(group->fec.ssrc)!=outgoing.end())
			return Error("-AddOutgoingSourceGroup fec ssrc already assigned");
		if (outgoing.find(group->rtx.ssrc)!=outgoing.end())
			return Error("-AddOutgoingSourceGroup rtx ssrc already assigned");
		
		//Add it for each group ssrc
		outgoing[group->media.ssrc] = group;
		if (group->fec.ssrc)
			outgoing[group->fec.ssrc] = group;
		if (group->rtx.ssrc)
			outgoing[group->rtx.ssrc] = group;
	}
	
	//If we don't have a mainSSRC
	if (mainSSRC==1)
		//Set it
		mainSSRC = group->media.ssrc;

	//Create rtcp sender retpor
	auto rtcp = RTCPCompoundPacket::Create(group->media.CreateSenderReport(getTime()));
	//Send packet
	Send(rtcp);
	
	//Done
	return true;
	
}

bool DTLSICETransport::RemoveOutgoingSourceGroup(RTPOutgoingSourceGroup *group)
{
	Log("-RemoveOutgoingSourceGroup [ssrc:%u,fec:%u,rtx:%u]\n",group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);

	//Scoped sync
	{
		//Lock outgoing
		ScopedUseLock use(outgoingUse);
		
		//Remove it from each ssrc
		outgoing.erase(group->media.ssrc);
		if (group->fec.ssrc)
			outgoing.erase(group->fec.ssrc);
		if (group->rtx.ssrc)
			outgoing.erase(group->rtx.ssrc);
		
		//If it was our main ssrc
		if (mainSSRC==group->media.ssrc)
			//Set first
			mainSSRC = outgoing.begin()!=outgoing.end() ? outgoing.begin()->second->media.ssrc : 1;
	}
	
	//Get ssrcs
	std::vector<DWORD> ssrcs;
	//Add group ssrcs
	ssrcs.push_back(group->media.ssrc);
	if (group->fec.ssrc) ssrcs.push_back(group->fec.ssrc);
	if (group->rtx.ssrc) ssrcs.push_back(group->rtx.ssrc);
	
	//Send BYE
	Send(RTCPCompoundPacket::Create(RTCPBye::Create(ssrcs,"terminated")));
	
	//Done
	return true;
}

bool DTLSICETransport::AddIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-AddIncomingSourceGroup [mid:'%s',rid:'%s',ssrc:%u,fec:%u,rtx:%u]\n",group->mid.c_str(),group->rid.c_str(),group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc && group->rid.empty())
		return Error("No media ssrc or rid defined, stream will not be added\n");
	
	//Use
	ScopedUse use(incomingUse);	
	
	//Check they are not already assigned
	if (group->media.ssrc && incoming.find(group->media.ssrc)!=incoming.end())
		return Error("-AddIncomingSourceGroup media ssrc already assigned");
	if (group->fec.ssrc && incoming.find(group->fec.ssrc)!=incoming.end())
		return Error("-AddIncomingSourceGroup fec ssrc already assigned");
	if (group->rtx.ssrc && incoming.find(group->rtx.ssrc)!=incoming.end())
		return Error("-AddIncomingSourceGroup rtx ssrc already assigned");

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
	if (group->media.ssrc)
		incoming[group->media.ssrc] = group;
	if (group->fec.ssrc)
		incoming[group->fec.ssrc] = group;
	if (group->rtx.ssrc)
		incoming[group->rtx.ssrc] = group;
	
	//If it is video and the transport wide cc is not enabled enable remb
	bool remb = group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)==RTPMap::NotFound;
	
	//Start distpaching
	group->Start(remb);
	
	//Done
	return true;
}

bool DTLSICETransport::RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-RemoveIncomingSourceGroup [mid:'%s',rid:'%s',ssrc:%u,fec:%u,rtx:%u]\n",group->mid.c_str(),group->rid.c_str(),group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//Stop distpaching
	group->Stop();
	
	//Block
	ScopedUseLock lock(incomingUse);

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
		
	
	//Remove it from each ssrc
	if (group->media.ssrc)
		incoming.erase(group->media.ssrc);
	if (group->fec.ssrc)
		incoming.erase(group->fec.ssrc);
	if (group->rtx.ssrc)
		incoming.erase(group->rtx.ssrc);
	
	//Done
	return true;
}

void DTLSICETransport::Send(const RTCPCompoundPacket::shared &rtcp)
{
	BYTE 	data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD   size = MTU;
	
	//Double check message
	if (!rtcp)
		//Error
		return (void)Error("-DTLSICETransport::Send() | NULL rtcp message\n");
	
	//Check if we have an active DTLS connection yet
	if (!send)
		//Error
		return (void) Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
	//Serialize
	DWORD len = rtcp->Serialize(data,size);
	
	//Check result
	if (len<=0 || len>size)
		//Error
		return (void)Error("-DTLSICETransport::Send() | Error serializing RTCP packet [len:%d]\n",len);
	
	
	ICERemoteCandidate* candidate;
	//Synchronized
	{
		//Block scope
		ScopedLock scope(mutex);
		
		//If we don't have an active candidate yet
		if (!active)
			//Error
			return (void) Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");

		//If dumping
		if (dumper && dumpRTCP)
			//Write udp packet
			dumper->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);
		
		//Encript
		srtp_err_status_t srtp_err_status = srtp_protect_rtcp(send,data,(int*)&len);
		
		//Check error
		if (srtp_err_status!=srtp_err_status_ok)
			//Error
			return (void)Error("-DTLSICETransport::Send() | Error protecting RTCP packet [%d]\n",srtp_err_status);
		
		//Store active candidate
		candidate = active;
		
		//Update bitrate
		outgoingBitrate.Update(getTimeMS(),len);
	}
		
	//No error yet, send packet
	int ret = sender->Send(candidate,data,len);

	//Check
	if (ret<=0)
		Error("-DTLSICETransport::Send() | Error sending RTCP packet [ret:%d,%d]\n",ret,errno);
}

int DTLSICETransport::SendPLI(DWORD ssrc)
{
	//Log
	Debug("-DTLSICETransport::SendPLI() | [ssrc:%u]\n",ssrc);
	
	//Sync scope
	{
		//Using incoming
		ScopedUse use(incomingUse);

		//Get group
		RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);

		//If not found
		if (!group)
			return Error("- DTLSICETransport::SendPLI() | no incoming source found for [ssrc:%u]\n",ssrc);
		
		//Check if we have sent a PLI recently (less than half a second ago)
		if (getTimeDiff(group->media.lastPLI)<1E6/2)
			//We refuse to end more
			return 0;
		//Update last PLI requested time
		group->media.lastPLI = getTime();
		//And number of requested plis
		group->media.totalPLIs++;
		
		//Drop all pending and lost packets
		group->ResetPackets();
		
	}
	
	//Create rtcp sender retpor
	auto rtcp = RTCPCompoundPacket::Create();
	
	//Add to rtcp
	rtcp->CreatePacket<RTCPPayloadFeedback>(RTCPPayloadFeedback::PictureLossIndication,mainSSRC,ssrc);

	//Send packet
	Send(rtcp);
	
	return 1;
}

int DTLSICETransport::Send(const RTPPacket::shared& packet)
{
	BYTE 	data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD	size = MTU;
	
	//Check packet
	if (!packet)
		//Error
		return Debug("-DTLSICETransport::Send() | Error null packet\n");
	
	//Check if we have an active DTLS connection yet
	if (!send)
		//Error
		return Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
	//Get ssrc
	DWORD ssrc = packet->GetSSRC();
	
	//Using outgoing
	ScopedUse use(outgoingUse);
	
	//Get outgoing group
	RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
	
	//If not found
	if (!group)
		//Error
		return Error("-DTLSICETransport::Send() | Outgoind source not registered for ssrc:%u\n",packet->GetSSRC());
	
	//Get outgoing source
	RTPOutgoingSource& source = group->media;
	
	//Clone packet
	auto cloned = packet->Clone();
	
        //SYNCHRONIZED
        {
                //Block scope
		ScopedLock scope(source);
                //Update headers
                cloned->SetExtSeqNum(source.CorrectExtSeqNum(cloned->GetExtSeqNum()));
                cloned->SetSSRC(source.ssrc);
        }
        
	cloned->SetPayloadType(sendMaps.rtp.GetTypeForCodec(cloned->GetCodec()));
	//No padding
	cloned->SetPadding(0);

	//Add transport wide cc on video
	if (group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
		//Set transport wide seq num
		cloned->SetTransportSeqNum(++transportSeqNum);
	else
		//Disable transport wide cc
		cloned->DisableTransportSeqNum();

	//If we are using abs send time for sending
	if (sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::AbsoluteSendTime)!=RTPMap::NotFound)
		//Set abs send time
		cloned->SetAbsSentTime(getTimeMS());
	else
		//Disable it
		cloned->DisableAbsSentTime();
	
	//Disable rid & repair id
	cloned->DisableRId();
	cloned->DisableRepairedId();
	
	//Update mid
	if (!group->mid.empty())
		//Set new mid
		cloned->SetMediaStreamId(group->mid);
	else
		//Disable it
		cloned->DisableMediaStreamId();
	
	//if (group->type==MediaFrame::Video) UltraDebug("-Sending RTP on media:%s sssrc:%u seq:%u pt:%u ts:%lu codec:%s\n",MediaFrame::TypeToString(group->type),source.ssrc,cloned->GetSeqNum(),cloned->GetPayloadType(),cloned->GetTimestamp(),GetNameForCodec(group->type,cloned->GetCodec()));
			
	//Serialize data
	int len = cloned->Serialize(data,size,sendMaps.ext);
	
	//IF failed
	if (!len)
		return 0;

	//Add packet for RTX
	group->AddPacket(cloned);
	
	ICERemoteCandidate* candidate;
	//Synchronized
	{
		//Block method
		ScopedLock method(mutex);
		
		//If we don't have an active candidate yet
		if (!active)
			//Error
			return Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");
		
		//If dumping
		if (dumper && dumpOutRTP)
			//Write udp packet
			dumper->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);
		
		//Encript
		srtp_err_status_t err = srtp_protect(send,data,&len);
		//Check error
		if (err!=srtp_err_status_ok)
			//Error
			return Error("-RTPTransport::SendPacket() | Error protecting RTP packet [%d]\n",err);
	
		//Store candidate
		candidate = active;
				
		//Update bitrate
		outgoingBitrate.Update(getTimeMS(),len);
	}
	
	//No error yet, send packet
	len = sender->Send(candidate,data,len);
	
	//If got packet to send
	if (len>0)
	{
                {
                        //Block scope
                        ScopedLock scope(source);
                        //Update last items
                        source.lastTime		= cloned->GetTimestamp();
                        source.lastPayloadType  = cloned->GetPayloadType();

                        //Update source
                        source.Update(getTimeMS(),cloned->GetSeqNum(),len);
                }

		//Check if we are using transport wide for this packet
		if (cloned->HasTransportWideCC())
		{
			//Block method
			ScopedLock method(transportWideMutex);
			//Add new stat
			transportWideSentPacketsStats[cloned->GetTransportSeqNum()] = PacketStats::Create(cloned,len,getTime());
			//Protect against missing feedbacks, remove too old lost packets
			auto it = transportWideSentPacketsStats.begin();
			//If we have more than 1s diff
			while(it!=transportWideSentPacketsStats.end() && getTimeDiff(it->second->time)>1E6)
				//Erase it and move iterator
				it = transportWideSentPacketsStats.erase(it);
		}
	} else {
		//Error
		Error("-DTLSICETransport::Send() | Error sending packet [len:%d,err:%d]\n",len,errno);
	}
	
	//Get time for packets to discard, always have at least 200ms, max 500ms
	QWORD until = getTimeMS() - (200+fmin(rtt*2,300));
	
	//Release packets from rtx queue
	group->ReleasePackets(until);
	
	//Check if we need to send SR (1 per second)
	if (getTimeDiff(source.lastSenderReport)>1E6)
		//Create and send rtcp sender retpor
		Send(RTCPCompoundPacket::Create(group->media.CreateSenderReport(getTime())));
	
	//Do we need to send probing?
	if (probe && group->type == MediaFrame::Video && cloned->GetMark() && source.remb)
	{
                DWORD bitrate   = 0;
		DWORD estimated = 0;
		
                //SYNCHRONIZED
                {
                        //Lock in scope
                        ScopedLock scope(source);
                        //Get bitrates
                        bitrate   = static_cast<DWORD>(source.acumulator.GetInstantAvg());
                        estimated = source.remb;
                }
		
					
		//If we can still send more
		if (estimated>bitrate)
		{
			BYTE size = 255;
			//Get probe padding needed
			DWORD probingBitrate = maxProbingBitrate ? std::min(estimated-bitrate,maxProbingBitrate) : estimated-bitrate;

			//Get number of probes, do not send more than 32 continoues packets (~aprox 2mpbs)
			BYTE num = std::min((probingBitrate*33)/(8000*size),32u);
			
			//UltraDebug("-DTLSICETransport::Run() | Sending inband probing packets [at:%u,estimated:%u,bitrate:%u,probing:%u,max:%u,num:%d]\n", cloned->GetSeqNum(), estimated, bitrate,probingBitrate,maxProbingBitrate, num, sleep);
			
			//Set all the probes
			for (BYTE i=0;i<num;++i)
				//Send probe packet
				SendProbe(group,size);
		}
	}
	
	//Did we send successfully?
	return (len>=0);
}

void DTLSICETransport::onRTCP(const RTCPCompoundPacket::shared& rtcp)
{
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
				//Using incoming
				ScopedUse use1(incomingUse);
				//Using outgoing
				ScopedUse use2(outgoingUse);
	
				auto sr = std::static_pointer_cast<RTCPSenderReport>(packet);
				
				//Get ssrc
				DWORD ssrc = sr->GetSSRC();
				
				//Get source
				RTPIncomingSource* source = GetIncomingSource(ssrc);
				
				//If not found
				if (!source)
				{
					Error("-Could not find incoming source for RTCP SR [ssrc:%u]\n",ssrc);
					rtcp->Dump();
					continue;
				}
				
				//Store info
				source->lastReceivedSenderNTPTimestamp = sr->GetNTPTimestamp();
				source->lastReceivedSenderReport = getTime();
				
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
							//Calculate RTT
							if (source->IsLastSenderReportNTP(report->GetLastSR()))
							{
								//Calculate new rtt in ms
								DWORD rtt = getTimeDiff(source->lastSenderReport)/1000-report->GetDelaySinceLastSRMilis();
								//Update packet jitter buffer
								SetRTT(rtt);
							}
						}
					}
				}
				break;
			}
			case RTCPPacket::ReceiverReport:
			{
				//Using outgoing
				ScopedUse use2(outgoingUse);
				
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
							//Calculate RTT
							if (source->IsLastSenderReportNTP(report->GetLastSR()))
							{
								//Calculate new rtt in ms
								DWORD rtt = getTimeDiff(source->lastSenderReport)/1000-report->GetDelaySinceLastSRMilis();
								//Set it
								SetRTT(rtt);
								
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
				break;
			case RTCPPacket::App:
				break;
			case RTCPPacket::RTPFeedback:
			{
				//Using outgoing
				ScopedUse use2(outgoingUse);
				
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
					Error("-DTLSICETransport::onRTCP() | Got feedback message for unknown media  [ssrc:%u]\n",ssrc);
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
							//Lost count
							QWORD last = field->referenceTime;
							//Block method
							ScopedLock lock(transportWideMutex);
							//For each packet
							for (auto& remote : field->packets)
							{
								//Get packets stats
								auto it = transportWideSentPacketsStats.find(remote.first);
								//If found
								if (it!=transportWideSentPacketsStats.end())
								{
									//Get stats
									auto stat = it->second;
									//Check if it was lost
									if (remote.second)
									{
										//UltraDebug("-Update seq:%d,sent:%llu,ts:%ll,size:%d\n",remote.first,remote.second/1000,stat->time/1000,stat->size);
										//Add it to sender side estimator
										senderSideEstimator.Update(0,remote.second/1000,stat->time/1000,stat->size,stat->mark);
										//Update last
										last = remote.second;
									} else {
										//Updateu
										senderSideEstimator.UpdateLost(0,1,last/1000);
									}
									//Erase it
									transportWideSentPacketsStats.erase(it);
								}
							}
						}
						break;
				}
				break;
			}
			case RTCPPacket::PayloadFeedback:
			{
				//Using outgoing
				ScopedUse use2(outgoingUse);
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
		//Get time
		QWORD time = stats->time;

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
	
	//We are running
	running = true;

	//Reset packet wait
	wait.Reset();
	
	//Create thread
	createPriorityThread(&thread,[](void* par)->void*{
		//Block signals to avoid exiting on SIGUSR1
		blocksignals();

		//Obtenemos el parametro
		DTLSICETransport *transport = (DTLSICETransport *)par;

		//Ejecutamos
		transport->Run();

		//Exit
		return NULL;
	},this,0);
}

void DTLSICETransport::Stop()
{
	Debug(">DTLSICETransport::Stop()\n");
	
	//Check thred
	if (!isZeroThread(thread))
	{
		//Not running
		running = false;
		
		//Cancel packets wait queue
		wait.Cancel();
		
		//Wait thread to close
		pthread_join(thread,NULL);
		//Nulifi thread
		setZeroThread(&thread);
	}
	
	Debug("<DTLSICETransport::Stop()\n");
}

int DTLSICETransport::Enqueue(const RTPPacket::shared& packet)
{
	ScopedLock lock(wait);
	
	//Add packet
	packets.push(packet);
		
	//Signal new element
	wait.Signal();
	
	return 1;
}

int DTLSICETransport::Run()
{
	Log(">DTLSICETransport::Run() | [%p]\n",this);
	
	//Lock for waiting for packets
	wait.Lock();
	
	//Get start time
	QWORD ini = getTime();
	
	//Start probing timer
	timeval time;
	getUpdDifTime(&time);
	
	//Run until canceled
	while(running)
	{
		//Get sleeping time
		QWORD sleep = 1000;
		//Get time since last probe
		QWORD diff = 0;
		//Endure that transport wide cc is enabled
		if ( probe && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC)!=RTPMap::NotFound)
		{
			//Get sleeping time
			sleep = 33;
			//Get time since last probe
			diff = getDifTime(&time)/1000;

			//If we are probing
			if (diff>=sleep)
			{
				ScopedUse use(outgoingUse);
				//Probe size
				BYTE size = 255;
				//Get bitrates
				DWORD bitrate   = static_cast<DWORD>(outgoingBitrate.GetInstantAvg());
				DWORD estimated = senderSideEstimator.GetEstimatedBitrate();

				//Probe the first 5s to 312kbps
				if (getTime()-ini<5E6)
					//Increase stimation
					estimated = std::max(estimated,bitrate+3120000);

				//If we can still send more
				if (estimated>bitrate)
				{
					//Get probe padding needed
					DWORD probingBitrate = maxProbingBitrate ? std::min(estimated-bitrate,maxProbingBitrate) : estimated-bitrate;

					//Get number of probes, do not send more than 32 continoues packets (~aprox 2mpbs)
					BYTE num = std::min((probingBitrate*sleep)/(8000*size),static_cast<QWORD>(32));

					//Check if we have an outgpoing group
					for (auto &group : outgoing)
					{
						//We can only probe on rtx with video
						if (group.second->type == MediaFrame::Video)
						{
							//UltraDebug("-DTLSICETransport::Run() | Sending probing packets [estimated:%u,bitrate:%u,probing:%u,max:%u,num:%d,sleep:%d]\n", estimated, bitrate,probingBitrate,maxProbingBitrate, num, sleep);
							//Set all the probes
							for (BYTE i=0;i<num;++i)
								//Send probe packet
								SendProbe(group.second,size);
							break;
						}
					}
				}

				//Reset timer
				getUpdDifTime(&time);
				diff = 0;
			}
		}
		
		//Check if we have packets
		if (packets.empty())
		{
			//Wait for more
			wait.Wait(sleep-diff);
			//Try again
			continue;
		}
		
		//Get first packet
		auto packet = packets.front();

		//Remove packet from list
		packets.pop();
		
		//UltraDebug("-last seq:%lu ts:%llu\n",lastExtSeqNum,lastTimestamp);
		
		//Unlock
		wait.Unlock();
		
		//Send it on transport
		Send(packet);
		
		//Lock for waiting for packets
		wait.Lock();
	}
	
	//Unlock
	wait.Unlock();
	
	Log("<DTLSICETransport::Run()\n");
	
	return 0;
}

void DTLSICETransport::SetBandwidthProbing(bool probe)
{
	//Lock wait
	wait.Lock();
	//Set probing status
	this->probe = probe;
	//Wake up run thread
	wait.Signal();
	//Unlock wait
	wait.Unlock();
}
