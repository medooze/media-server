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
#include "DTLSICETransport.h"
#include "rtp/RTPMap.h"
#include "rtp/RTPHeader.h"
#include "rtp/RTPHeaderExtension.h"

DTLSICETransport::DTLSICETransport(Sender *sender) : dtls(*this), mutex(true)
{
	//Store sender
	this->sender = sender;
	//No active candidate
	active = NULL;
	//SRTP instances
	send = NULL;
	recv = NULL;
	//Transport wide seq num
	transportSeqNum = 1;
	feedbackPacketCount = 1;
	feedbackCycles = 0;
	lastFeedbackPacketExtSeqNum = 0;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	//Init our main ssrc
	mainSSRC = 1;
	//Not dumping
	pcap = NULL;
	//Add dummy stream to stimator
	senderSideEstimator.AddStream(0);
}

DTLSICETransport::~DTLSICETransport()
{
	//Reset
	Reset();
}

int DTLSICETransport::onData(const ICERemoteCandidate* candidate,BYTE* data,DWORD size)
{
	RTPHeader header;
	RTPHeaderExtension extension;
	DWORD len = size;
	
	//Check if it a DTLS packet
	if (DTLSConnection::IsDTLS(data,size))
	{
		//Feed it
		dtls.Write(data,size);

		//Read
		//Buffers are always MTU size
		DWORD len = dtls.Read(data,MTU);

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
		if (pcap)
			//Write udp packet
			pcap->WriteUDP(getTimeMS(),candidate->GetIPAddress(),candidate->GetPort(),0x7F000001,5004,data,len);
		
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(data,len);
	
		//Check packet
		if (!rtcp)
		{
			//Debug
			Debug("-RTPBundleTransport::onData() | RTCP wrong data\n");
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
		header.Parse(data,size);
		header.Dump();
		//Error
		return 0;
	}
	
	//If dumping
	if (pcap)
		//Write udp packet
		pcap->WriteUDP(getTimeMS(),candidate->GetIPAddress(),candidate->GetPort(),0x7F000001,5004,data,len);
	
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
			Debug("-RTPSession::onRTPPacket() | RTP padding is bigger than size [padding:%u,size%u]\n",padding,len);
			//Exit
			return 0;
		}
		//Remove from size
		len -= padding;
	}
	
	//Get ssrc
	DWORD ssrc = header.ssrc;
	
	//Using incoming
	ScopedUse use(incomingUse);
	
	//Get group
	RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);
	
	//If it doesn't have a group but does hava an rtp stream id
	if (!group && extension.hasRTPStreamId)
	{
		Debug("-DTLSICETransport::onData() | Unknowing group for ssrc trying to retrieve by [ssrc:%u,rid:'%s']\n",ssrc,extension.rid.c_str());
		//Try to find it on the rids
		auto it = rids.find(extension.rid);
		//If found
		if (it!=rids.end())
		{
			Log("-DTLSICETransport::onData() | Associating rtp stream id to ssrc [ssrc:%u,rid:'%s']\n",ssrc,extension.rid.c_str());
					
			//Got source
			group = it->second;
			
			//Set ssrc for next ones
			//TODO: ensure it didn't had a previous ssrc
			group->media.ssrc = ssrc;
			
			//Add it to the incoming list
			incoming[group->media.ssrc] = group;
		}
	}
			
	//Ensure it has a group
	if (!group)	
		//error
		return Error("-DTLSICETransport::onData() | Unknowing group for ssrc [%u]\n",ssrc);
	
	//Get source for ssrc
	RTPIncomingSource* source = group->GetSource(ssrc);
	
	//Ensure it has a source
	if (!source)
		//error
		return Error("-DTLSICETransport::onData() | Group does not contain ssrc [%u]\n",ssrc);

	//Get initial codec
	BYTE codec = recvMaps.rtp.GetCodecForType(header.payloadType);
	
	//Check codec
	if (codec==RTPMap::NotFound)
		//Exit
		return Error("-DTLSICETransport::onData() | RTP packet type unknown [%d]\n",header.payloadType);
	
	//UUltraDebug("-Got RTP on media:%s sssrc:%u seq:%u pt:%u codec:%s rid:'%s'\n",MediaFrame::TypeToString(group->type),ssrc,header.sequenceNumber,header.payloadType,GetNameForCodec(group->type,codec),group->rid.c_str());
	
	//Create normal packet
	RTPPacket *packet = new RTPPacket(group->type,codec,header,extension);
	
	//Set the payload
	packet->SetPayload(data+ini,len-ini);
	
	//Update source
	source->Update(getTimeMS(),header.sequenceNumber,size);
	
	//Set cycles back
	packet->SetSeqCycles(source->cycles);
	
	//If it is video and transport wide cc is used
	if (group->type == MediaFrame::Video && packet->HasTransportWideCC())
	{
		//TODO: Calculate seqnum seq wrap here better!!!!!!!! <------------------------------------------------------------------------------ SERGIO------------------------------------------
		//Add packets to the transport wide stats
		transportWideReceivedPacketsStats[packet->GetTransportSeqNum()] = PacketStats::Create(*packet,size,getTime());
		
		//If we have enought or timeout (500ms)
		//TODO: Implement timer
		if (transportWideReceivedPacketsStats.size()>20 || (getTime()-transportWideReceivedPacketsStats.begin()->second->time)>5E5)
			//Send feedback message
			SendTransportWideFeedbackMessage(ssrc);
	}
	
	//If it was an RTX packet
	if (ssrc==group->rtx.ssrc) 
	{
		//Ensure that it is a RTX codec
		if (codec!=VideoCodec::RTX)
			//error
			return  Error("-DTLSICETransport::onData() | No RTX codec on rtx sssrc:%u type:%d codec:%d\n",packet->GetSSRC(),packet->GetPayloadType(),packet->GetCodec());
		
		 //Find codec for apt type
		 codec = recvMaps.apt.GetCodecForType(packet->GetPayloadType());
		//Check codec
		 if (codec==RTPMap::NotFound)
			  //Error
			  return Error("-DTLSICETransport::ReadRTP(%s) | RTP RTX packet apt type unknown [%d]\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType());
		/*
		   The format of a retransmission packet is shown below:
		    0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                         RTP Header                            |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |            OSN                |                               |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
		   |                  Original RTP Packet Payload                  |
		   |                                                               |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */
		 //Get original sequence number
		 WORD osn = get2(packet->GetMediaData(),0);

		 UltraDebug("RTX: %s got   %.d:RTX for #%d ts:%u payload:%u\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType(),osn,packet->GetTimestamp(),packet->GetMediaLength());
		 
		 //Set original seq num
		 packet->SetSeqNum(osn);
		 //Set original ssrc
		 packet->SetSSRC(group->media.ssrc);
		 //Set cycles
		 packet->SetSeqCycles(group->media.cycles);
		 //Set codec
		 packet->SetCodec(codec);
		 
		 //Skip osn from payload
		 packet->SkipPayload(2);
		 
		 //TODO: We should set also the original type
		  UltraDebug("RTX: %s got   %.d:RTX for #%d ts:%u %u %u payload:%u\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType(),osn,packet->GetTimestamp(),packet->GetExtSeqNum(),group->media.extSeq,packet->GetMediaLength());
		 //WTF! Drop RTX packets for the future (UUH)
		 if (packet->GetExtSeqNum()>group->media.extSeq)
		 {
			 //DO NOTHING with it yet
			delete(packet);
			//Error
			return Error("Drop RTX future packet [osn:%u,max:%u]\n",osn,group->media.extSeq>>16);
		 }
		 
	} else if (ssrc==group->fec.ssrc)  {
		UltraDebug("-Flex fec\n");
		//Ensure that it is a FEC codec
		if (codec!=VideoCodec::FLEXFEC)
			//error
			return  Error("-DTLSICETransport::onData() | No FLEXFEC codec on fec sssrc:%u type:%d codec:%d\n",MediaFrame::TypeToString(packet->GetMedia()),packet->GetPayloadType(),packet->GetSSRC());
		//DO NOTHING with it yet
		delete(packet);
		return 1;
	}	

	//Update lost packets
	DWORD lost = group->losts.AddPacket(packet);
	
	//Get total lost in queue
	DWORD total = group->losts.GetTotal();

	//Request NACK if it is media
	if (group->type == MediaFrame::Video && lost && ssrc==group->media.ssrc)
	{
		UltraDebug("-lost %d\n",total);
		
		//Create rtcp sender retpor
		RTCPCompoundPacket rtcp;
		
		//Create report
		RTCPReport *report = source->CreateReport(getTime());
		
		//Create sender report for normal stream
		RTCPReceiverReport* rr = new RTCPReceiverReport(mainSSRC);

		//If got anything
		if (report)
			//Append it
			rr->AddReport(report);

		//Append RR to rtcp
		rtcp.AddRTCPacket(rr);
	
		//Get nacks for lost
		std::list<RTCPRTPFeedback::NACKField*> nacks = group->losts.GetNacks();

		//Create NACK
		RTCPRTPFeedback *nack = RTCPRTPFeedback::Create(RTCPRTPFeedback::NACK,mainSSRC,ssrc);

		//Add 
		for (std::list<RTCPRTPFeedback::NACKField*>::iterator it = nacks.begin(); it!=nacks.end(); ++it)
			//Add it
			nack->AddField(*it);

		//Add to packet
		rtcp.AddRTCPacket(nack);

		//Send packet
		Send(rtcp);
	}
  
	RTPPacket* ordered;
	//Append it to the packets
	group->packets.Add(packet);
	//FOr each ordered packet
	while((ordered=group->packets.GetOrdered()))
		//Call listeners
		group->onRTP(ordered);
	
	//TODO: Fix who deletes packet
	
	//Check if we need to send RR (1 per second)
	if (getTimeDiff(source->lastReport)>1E6)
	{
		//Create rtcp sender retpor
		RTCPCompoundPacket rtcp;
		
		//Create report
		RTCPReport *report = source->CreateReport(getTime());
		
		//Create sender report for normal stream
		RTCPReceiverReport* rr = new RTCPReceiverReport(mainSSRC);

		//If got anything
		if (report)
			//Append it
			rr->AddReport(report);
		
		//Add report
		rtcp.AddRTCPacket(rr);
		
		//Send it
		Send(rtcp);
	}
	//Done
	return 1;
}

void DTLSICETransport::ReSendPacket(RTPOutgoingSourceGroup *group,WORD seq)
{
	UltraDebug("-DTLSICETransport::ReSendPacket() | resending [seq:%d,ssrc:%u]\n",seq,group->rtx.ssrc);
	
	//Calculate ext seq number
	//TODO: consider warp
	DWORD ext = ((DWORD)(group->media.cycles)<<16 | seq);

	//Find packet to retransmit
	auto it = group->packets.find(ext);

	//If we still have it
	if (it==group->packets.end())
		//End
		return;
	
	//Get packet
	RTPPacket* packet = it->second;

	//Get outgoing source
	RTPOutgoingSource& source = group->rtx;

	//Data
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	DWORD size = MTU;
	int len = 0;

	//Overrride headers
	RTPHeader		header(packet->GetRTPHeader());
	RTPHeaderExtension	extension(packet->GetRTPHeaderExtension());

	//If it is using rtx (i.e. not firefox)
	if (source.ssrc)
	{
		//Update RTX headers
		header.ssrc		= source.ssrc;
		header.payloadType	= sendMaps.rtp.GetTypeForCodec(VideoCodec::RTX);
		header.sequenceNumber	= source.extSeq++;
		//No padding
		header.padding		= 0;

		//Calculate last timestamp
		source.lastTime =  packet->GetTimestamp();
	}

	//Add transport wide cc on video
	if (group->type == MediaFrame::Video)
	{
		//Add extension
		header.extension = true;
		//Add transport
		extension.hasTransportWideCC = true;
		extension.transportSeqNum = transportSeqNum++;
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
	if (source.ssrc)
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
		if (pcap)
			//Write udp packet
			pcap->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);

		//Encript
		srtp_err_status_t err = srtp_protect(send,data,(int*)&len);
		//Check error
		if (err!=srtp_err_status_ok)
			//Error
			return (void)Error("-RTPTransport::SendPacket() | Error protecting RTP packet [%d]\n",err);

		//Store candidate before unlocking
		candidate = active;
	}

	//No error yet, send packet
	if (sender->Send(candidate,data,len)<=0)
		//Error
		Error("-RTPTransport::SendPacket() | Error sending RTP packet [%d]\n",errno);
	
	//Update rtx
	if (source.ssrc)
		//Update stats
		source.Update(getTimeMS(),header.sequenceNumber,len);
}

void  DTLSICETransport::ActivateRemoteCandidate(ICERemoteCandidate* candidate,bool useCandidate, DWORD priority)
{
	//Lock
	mutex.Lock();
	
	//Debug
	UltraDebug("-DTLSICETransport::ActivateRemoteCandidate() | Remote candidate [%s:%hu,use:%d,prio:%d]\n",candidate->GetIP(),candidate->GetPort(),useCandidate,priority);
	
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
				sendMaps.apt[rtx] = codec;
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
				recvMaps.apt[rtx] = codec;
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
		
int DTLSICETransport::Dump(const char* filename)
{
	
	Log("-DTLSICETransport::Dump()\n");
	
	if (pcap)
		return Error("Already dumping\n");
	
	//Create pcap file
	pcap = new PCAPFile();
	
	//Open it
	if (!pcap->Open(filename))
	{
		//Error
		delete(pcap);
		pcap = NULL;
		return 0;
	}
	
	//Ok
	return true;
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
	if (pcap)
		//Delete it
		delete(pcap);
	
	send = NULL;
	recv = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	pcap = NULL;
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
		Log("-RTPBundleTransport::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-RTPBundleTransport::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-RTPBundleTransport::SetLocalCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-RTPBundleTransport::SetLocalCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else {
		return Error("-RTPBundleTransport::SetLocalCryptoSDES() | Unknown cipher suite: %s", suite);
	}

	//Check sizes
	if (len!=(DWORD)policy.rtp.cipher_key_len)
		//Error
		return Error("-RTPBundleTransport::SetLocalCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

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
		return Error("-RTPBundleTransport::SetLocalCryptoSDES() | Failed to create local SRTP session | err:%d\n", err);
	
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
	Log("-RTPBundleTransport::SetLocalSTUNCredentials() | [frag:%s,pwd:%s]\n",username,pwd);
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
	Log("-RTPBundleTransport::SetRemoteSTUNCredentials() |  [frag:%s,pwd:%s]\n",username,pwd);
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
	Log("-RTPBundleTransport::SetRemoteCryptoDTLS | [setup:%s,hash:%s,fingerprint:%s]\n",setup,hash,fingerprint);

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
		return Error("-RTPBundleTransport::SetRemoteCryptoDTLS | Unknown setup");

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
		return Error("-RTPBundleTransport::SetRemoteCryptoDTLS | Unknown hash");

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
		Log("-RTPBundleTransport::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-RTPBundleTransport::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-RTPBundleTransport::SetRemoteCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-RTPBundleTransport::SetRemoteCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else {
		return Error("-RTPBundleTransport::SetRemoteCryptoSDES() | Unknown cipher suite %s", suite);
	}

	//Check sizes
	if (len!=(DWORD)policy.rtp.cipher_key_len)
		//Error
		return Error("-RTPBundleTransport::SetRemoteCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

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
		return Error("-RTPBundleTransport::SetRemoteCryptoSDES() | Failed to create remote SRTP session | err:%d\n", err);
	
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
	Log("-RTPBundleTransport::onDTLSSetup()\n");

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
		case DTLSConnection::F8_128_HMAC_SHA1_80:
			//Set keys
			SetLocalCryptoSDES("NULL_CIPHER_HMAC_SHA1_80",localMasterKey,localMasterKeySize);
			SetRemoteCryptoSDES("NULL_CIPHER_HMAC_SHA1_80",remoteMasterKey,remoteMasterKeySize);
			break;
		default:
			Error("-RTPBundleTransport::onDTLSSetup() | Unknown suite\n");
	}
}

bool DTLSICETransport::AddOutgoingSourceGroup(RTPOutgoingSourceGroup *group)
{
	//Log
	Log("-AddOutgoingSourceGroup [ssrc:%u,fec:%u,rtx:%u]\n",group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//Scoped sync
	{
		//Using outgoing
		ScopedUse use(outgoingUse);
		
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
	RTCPCompoundPacket rtcp(group->media.CreateSenderReport(getTime()));
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
	//Create BYE
	RTCPCompoundPacket rtcp(RTCPBye::Create(ssrcs,"terminated"));
	//Send packet
	Send(rtcp);
	
	//Done
	return true;
}

bool DTLSICETransport::AddIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-AddIncomingSourceGroup [rid:'%s',ssrc:%u,fec:%u,rtx:%u]\n",group->rid.c_str(),group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc && group->rid.empty())
		return Error("No media ssrc or rid defined, stream will not be added\n");
	
	//Use
	ScopedUse use(incomingUse);	

	//Add rid if any
	if (!group->rid.empty())
		rids[group->rid] = group;
	
	//Add it for each group ssrc
	if (group->media.ssrc)
		incoming[group->media.ssrc] = group;
	if (group->fec.ssrc)
		incoming[group->fec.ssrc] = group;
	if (group->rtx.ssrc)
		incoming[group->rtx.ssrc] = group;
	//Done
	return true;
}

bool DTLSICETransport::RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-RemoveIncomingSourceGroup [ssrc:%u,fec:%u,rtx:%u]\n",group->media.ssrc,group->fec.ssrc,group->rtx.ssrc);
	
	//It must contain media ssrc
	if (!group->media.ssrc)
		return Error("No media ssrc defined, stream will not be removed\n");
	
	//Block
	ScopedUseLock lock(incomingUse);

	//Remove rid if any
	if (!group->rid.empty())
		rids.erase(group->rid);
	
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

void DTLSICETransport::Send(RTCPCompoundPacket &rtcp)
{
	BYTE 	data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD   size = MTU;
	
	//Check if we have an active DTLS connection yet
	if (!send)
		//Error
		return (void) Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
	//Serialize
	DWORD len = rtcp.Serialize(data,size);
	
	//Check result
	if (len<=0 || len>size)
		//Error
		return (void)Error("-DTLSICETransport::Send() | Error serializing RTCP packet [len:%d]\n",len);
	
	
	ICERemoteCandidate* candidate;
	//Synchronized
	{
		//Block scope
		ScopedLock method(mutex);
		
		//If we don't have an active candidate yet
		if (!active)
			//Error
			return (void) Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");

		//If dumping
		if (pcap)
			//Write udp packet
			pcap->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);
		
		//Encript
		srtp_err_status_t srtp_err_status = srtp_protect_rtcp(send,data,(int*)&len);
		//Check error
		if (srtp_err_status!=srtp_err_status_ok)
			//Error
			return (void)Error("-DTLSICETransport::Send() | Error protecting RTCP packet [%d]\n",srtp_err_status);
		
		//Store active candidate
		candidate = active;
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
	Log("-DTLSICETransport::SendPLI() | [ssrc:%u]\n",ssrc);
	
	//Sync scope
	{
		//Using incoming
		ScopedUse use(incomingUse);

		//Get group
		RTPIncomingSourceGroup *group = GetIncomingSourceGroup(ssrc);

		//If not found
		if (!group)
			return Error("- DTLSICETransport::SendPLI() | no incoming source found for [ssrc:%u]\n",ssrc);

		//Drop all pending and lost packets
		group->packets.Reset();
		group->losts.Reset();
	}
	
	//Create rtcp sender retpor
	RTCPCompoundPacket rtcp;
	
	//Add to rtcp
	rtcp.AddRTCPacket( RTCPPayloadFeedback::Create(RTCPPayloadFeedback::PictureLossIndication,mainSSRC,ssrc));

	//Send packet
	Send(rtcp);
	
	return 1;
}

int DTLSICETransport::Send(RTPPacket &packet)
{
	BYTE 	data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD	size = MTU;
	
	//Check if we have an active DTLS connection yet
	if (!send)
		//Error
		return Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
	//Get ssrc
	DWORD ssrc = packet.GetSSRC();
	
	//Using outgoing
	ScopedUse use(outgoingUse);
	
	//Get outgoing group
	RTPOutgoingSourceGroup* group = GetOutgoingSourceGroup(ssrc);
	
	//If not found
	if (!group)
		//Error
		return Error("-DTLSICETransport::Send() | Outgoind source not registered for ssrc:%u\n",packet.GetSSRC());
	
	//Get outgoing source
	RTPOutgoingSource& source = group->media;
	
	//Overrride headers
	RTPHeader		header(packet.GetRTPHeader());
	RTPHeaderExtension	extension(packet.GetRTPHeaderExtension());

	//Update RTX headers
	header.ssrc		= source.ssrc;
	header.payloadType	= sendMaps.rtp.GetTypeForCodec(packet.GetCodec());
	//No padding
	header.padding		= 0;

	//Add transport wide cc on video
	if (group->type == MediaFrame::Video && sendMaps.ext.GetTypeForCodec(RTPHeaderExtension::TransportWideCC))
	{
		//Add extension
		header.extension = true;
		//Add transport
		extension.hasTransportWideCC = true;
		extension.transportSeqNum = transportSeqNum++;
	}

	//Serialize header
	int len = header.Serialize(data,size);

	//Check
	if (!len)
		//Error
		return Error("-DTLSICETransport::SendPacket() | Error serializing rtp headers\n");

	//If we have extension
	if (header.extension)
	{
		//Serialize
		int n = extension.Serialize(sendMaps.ext,data+len,size-len);
		//Comprobamos que quepan
		if (!n)
			//Error
			return Error("-DTLSICETransport::SendPacket() | Error serializing rtp extension headers\n");
		//Inc len
		len += n;
	}

	//Ensure we have enougth data
	if (len+packet.GetMediaLength()>size)
		//Error
		return Error("-DTLSICETransport::SendPacket() | Media overflow\n");

	//Copiamos los datos
	memcpy(data+len,packet.GetMediaData(),packet.GetMediaLength());

	//Set pateckt length
	len += packet.GetMediaLength();

	//If there is an rtx stream
	if (group->rtx.ssrc)
	{
		//Create new packet with this data
		RTPPacket *rtx = new RTPPacket(packet.GetMedia(),packet.GetCodec(),header,extension);
		//Set media
		rtx->SetPayload(packet.GetMediaData(),packet.GetMediaLength());
		//Add a clone to the rtx queue
		group->packets[rtx->GetExtSeqNum()] = rtx;
	}
	
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
		if (pcap)
			//Write udp packet
			pcap->WriteUDP(getTimeMS(),0x7F000001,5004,active->GetIPAddress(),active->GetPort(),data,len);
		
		//Encript
		srtp_err_status_t err = srtp_protect(send,data,&len);
		//Check error
		if (err!=srtp_err_status_ok)
			//Error
			return Error("-RTPTransport::SendPacket() | Error protecting RTP packet [%d]\n",err);
	
		//Store candidate
		candidate = active;
	}
	
	//No error yet, send packet
	len = sender->Send(candidate,data,len);
	
	//If got packet to send
	if (len<=0)
		return Error("-DTLSICETransport::Send() | Error sending packet [len:%d,err:%d]\n",len,errno);

	//Calculate last timestamp
	source.lastTime	= packet.GetTimestamp();
	
	//Update source
	source.Update(getTimeMS(),packet.GetSeqNum(),len);

	//Check if we are using transport wide for this packet
	if (packet.HasTransportWideCC())
		//Add new stat
		transportWideSentPacketsStats[packet.GetTransportSeqNum()] = PacketStats::Create(packet,len,getTime());

	//Get time for packets to discard, always have at least 200ms, max 500ms
	QWORD until = getTimeMS() - (200+fmin(rtt*2,300));
	//Delete old packets
	auto it2 = group->packets.begin();
	//Until the end
	while(it2!=group->packets.end())
	{
		//Get pacekt
		RTPPacket *pkt = it2->second;
		//Check time
		if (pkt->GetTime()>until)
			//Keep the rest
			break;
		//DElete from queue and move next
		group->packets.erase(it2++);
		//Delete object
		delete(pkt);
	}
	
	//Check if we need to send SR (1 per second)
	if (getTimeDiff(source.lastSenderReport)>1E6)
	{
		//Create rtcp sender retpor
		RTCPCompoundPacket rtcp(group->media.CreateSenderReport(getTime()));
		//Send packet
		Send(rtcp);
	}
	
	return 1;
}

void DTLSICETransport::onRTCP(RTCPCompoundPacket* rtcp)
{
	//For each packet
	for (DWORD i = 0; i<rtcp->GetPacketCount();i++)
	{
		//Get pacekt
		const RTCPPacket* packet = rtcp->GetPacket(i);
		//Check packet type
		switch (packet->GetType())
		{
			case RTCPPacket::SenderReport:
			{
				//Using incoming
				ScopedUse use1(incomingUse);
				//Using outgoing
				ScopedUse use2(outgoingUse);
	
				const RTCPSenderReport* sr = (const RTCPSenderReport*)packet;
				
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
					RTCPReport *report = sr->GetReport(j);
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
				
				const RTCPReceiverReport* rr = (const RTCPReceiverReport*)packet;
				//Process all the receiver Reports
				for (DWORD j=0;j<rr->GetCount();j++)
				{
					//Get report
					RTCPReport *report = rr->GetReport(j);
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
				RTCPRTPFeedback *fb = (RTCPRTPFeedback*) packet;
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
							const RTCPRTPFeedback::NACKField *field = (const RTCPRTPFeedback::NACKField*) fb->GetField(i);
							
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
							DWORD lost = 0;
							QWORD last = field->referenceTime;
							//For each packet
							for (auto& remote : field->packets)
							{
								//Get packets stats
								auto stat = transportWideSentPacketsStats[remote.first];
								//If found
								if (stat)
								{
									//Check if it was lsot
									if (remote.second)
									{
										//Add it to sender side estimator
										senderSideEstimator.Update(0,remote.second/1000,stat->time,stat->size);
										//Update last
										last = remote.second;
									} else
										//Update lost
										lost++;
										
								}
							}
							//If any lost
							if (lost)
								//Update
								senderSideEstimator.UpdateLost(0,lost,last/1000);
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
				RTCPPayloadFeedback *fb = (RTCPPayloadFeedback*) packet;
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
					Error("-Got feedback message for unknown media  [ssrc:%u]\n",ssrc);
					//Ups! Skip
					continue;
				}
				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPPayloadFeedback::PictureLossIndication:
					case RTCPPayloadFeedback::FullIntraRequest:
						Debug("-DTLSICETransport::onRTCP() | FPU requested [ssrc:%u,group:%p,this:%p]\n",ssrc,group,this);
						//Call listeners
						group->onPLIRequest(ssrc);
						//Get media
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
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get feedback
							const RTCPPayloadFeedback::ApplicationLayerFeeedbackField* msg = (const RTCPPayloadFeedback::ApplicationLayerFeeedbackField*)fb->GetField(i);
							//Get size and payload
							DWORD len		= msg->GetLength();
							const BYTE* payload	= msg->GetPayload();
							//Check if it is a REMB
							if (len>8 && payload[0]=='R' && payload[1]=='E' && payload[2]=='M' && payload[3]=='B')
							{
								//TODO: Implement REMB support
								/*
								//GEt exponent
								BYTE exp = payload[5] >> 2;
								DWORD mantisa = payload[5] & 0x03;
								mantisa = mantisa << 8 | payload[6];
								mantisa = mantisa << 8 | payload[7];
								//Get bitrate
								DWORD bitrate = mantisa << exp;
								*/
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
	//Sore it
	this->rtt = rtt;
	//Update jitters
	for (auto it = incoming.begin(); it!=incoming.end(); ++it)
		//Update jitter
		it->second->packets.SetMaxWaitTime(fmin(60,rtt*2));
}

void DTLSICETransport::SendTransportWideFeedbackMessage(DWORD ssrc)
{
	//Create rtcp transport wide feedback
	RTCPCompoundPacket rtcp;

	//Add to rtcp
	RTCPRTPFeedback* feedback = RTCPRTPFeedback::Create(RTCPRTPFeedback::TransportWideFeedbackMessage,mainSSRC,ssrc);

	//Create trnasport field
	RTCPRTPFeedback::TransportWideFeedbackMessageField *field = new RTCPRTPFeedback::TransportWideFeedbackMessageField(feedbackPacketCount++);

	//And add it
	feedback->AddField(field);

	//Add it
	rtcp.AddRTCPacket(feedback);

	//Proccess and delete all elements
	for (auto it =transportWideReceivedPacketsStats.cbegin();
		  it!=transportWideReceivedPacketsStats.cend();
		  it = transportWideReceivedPacketsStats.erase(it))
	{
		//Get stat
		PacketStats* stats = it->second;
		//Get transport seq
		DWORD transportSeqNum = it->first;
		//Get time
		QWORD time = stats->time;

		//Check if we have a sequence wrap
		if (transportSeqNum<0x0FFF && (lastFeedbackPacketExtSeqNum & 0xFFFF)>0xF000)
			//Increase cycles
			feedbackCycles++;

		//Get extended value
		DWORD transportExtSeqNum = feedbackCycles<<16 | transportSeqNum;

		//if not first and not out of order
		if (lastFeedbackPacketExtSeqNum && transportExtSeqNum>lastFeedbackPacketExtSeqNum)
			//For each lost
			for (DWORD i = lastFeedbackPacketExtSeqNum+1; i<transportExtSeqNum; ++i)
				//Add it
				field->packets.insert(std::make_pair(i,0));
		//Store last
		lastFeedbackPacketExtSeqNum = transportExtSeqNum;

		//Add this one
		field->packets.insert(std::make_pair(transportSeqNum,time));

		//Delete stat
		delete(stats);
	}

	//Send packet
	Send(rtcp);
}