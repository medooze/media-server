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
#include <openssl/ossl_typ.h>

/*************************
* RTPSession
* 	Constructro
**************************/
RTPSession::RTPSession(MediaFrame::Type media,Listener *listener) :
	send(media,NULL),
	recv(media,NULL),
	transport(this),
	losts(640)
{
	//Store listener
	this->listener = listener;
	//And media
	this->media = media;
	//Init values
	sendType = -1;
	useRTCP = true;
	useAbsTime = false;
	recTimestamp = 0;
	setZeroTime(&recTimeval);
	firReqNum = 0;
	requestFPU = false;
	pendingTMBR = false;
	pendingTMBBitrate = 0;
	//Don't use PLI by default
	usePLI = false;
	
	//Default cname
	cname = strdup("default@localhost");
	
	//Empty types by defauilt
	rtpMapIn = NULL;
	rtpMapOut = NULL;
	//No reports
	setZeroTime(&lastFPU);
	setZeroTime(&lastReceivedSR);
	rtt = 0;
	
	//NO FEC
	useNACK = false;
	useAbsTime = false;
	isNACKEnabled = false;
	//Reduce jitter buffer to min
	packets.SetMaxWaitTime(60);

	//No stimator
	remoteRateEstimator = NULL;

}

/*************************
* ~RTPSession
* 	Destructor
**************************/
RTPSession::~RTPSession()
{
	//Reset
	Reset();
	
	//Check listener
	if (remoteRateEstimator)
		//Add as listener
		remoteRateEstimator->SetListener(NULL);
}

void RTPSession::Reset()
{
	Log("-RTPSession reset\n");
	
	//Free mem
	if (rtpMapIn)
		delete(rtpMapIn);
	if (rtpMapOut)
		delete(rtpMapOut);
	if (cname)
		free(cname);
	//Delete packets
	packets.Clear();
	//Reset transport
	transport.Reset();
	
	//Empty queue
	FlushRTXPackets();
	//Init values
	sendType = -1;
	useAbsTime = false;
	recTimestamp = 0;
	setZeroTime(&recTimeval);
	firReqNum = 0;
	requestFPU = false;
	pendingTMBR = false;
	pendingTMBBitrate = 0;
	
	//Empty types by defauilt
	rtpMapIn = NULL;
	rtpMapOut = NULL;
	//No reports
	setZeroTime(&lastFPU);
	setZeroTime(&lastReceivedSR);
	rtt = 0;
	
	//NO FEC
	useNACK = false;
	useAbsTime = false;
	isNACKEnabled = false;
	//Reduce jitter buffer to min
	packets.SetMaxWaitTime(60);
	
	//Reset stream soutces
	send.media.Reset();
	recv.media.Reset();
	send.rtx.Reset();
	recv.media.Reset();
}

void RTPSession::FlushRTXPackets()
{
	//Lock mutex inside the method
	ScopedLock method(sendMutex);
	
	Debug("-FlushRTXPackets(%s)\n",MediaFrame::TypeToString(media));

	//Delete rtx packets
	for (RTPOrderedPackets::iterator it = rtxs.begin(); it!=rtxs.end();++it)
	{
		//Get pacekt
		RTPPacket *pkt = it->second;
		//Delete object
		delete(pkt);
	}

	//Clear list
	rtxs.clear();
}

void RTPSession::SetSendingRTPMap(RTPMap &map)
{
	//If we already have one
	if (rtpMapOut)
		//Delete it
		delete(rtpMapOut);
	//Clone it
	rtpMapOut = new RTPMap(map);
}

int RTPSession::SetLocalCryptoSDES(const char* suite, const char* key64)
{
	//Log
	Log("-RTPSession::SetLocalCryptoSDES(%s) | [key:%s,suite:%s]\n",MediaFrame::TypeToString(media),key64,suite);

	//Set it on transport
	int ret = transport.SetLocalCryptoSDES(suite,key64);

	//Request an intra to start clean
	if (listener)
		//Request a I frame
		listener->onFPURequested(this);

	//Evrything ok
	return ret;
}

int RTPSession::SetProperties(const Properties& properties)
{
	//Clean extension map
	extMap.clear();

	//For each property
	for (Properties::const_iterator it=properties.begin();it!=properties.end();++it)
	{
		Log("-RTPSession::SetProperties(%s) | Setting RTP property [%s:%s]\n",MediaFrame::TypeToString(media),it->first.c_str(),it->second.c_str());

		//Check
		if (it->first.compare("rtcp-mux")==0)
		{
			//Set rtcp muxing
			transport.SetMuxRTCP( atoi(it->second.c_str()) );
		} else if (it->first.compare("useRTCP")==0) {
			//Set rtx
			useRTCP = atoi(it->second.c_str());
		} else if (it->first.compare("secure")==0) {
			//Encript and decript
			transport.SetSecure(true);
		} else if (it->first.compare("ssrc")==0) {
			//Set ssrc for sending
			send.media.ssrc = atoi(it->second.c_str());
		} else if (it->first.compare("ssrcRTX")==0) {
			//Set ssrc for sending
			send.rtx.ssrc = atoi(it->second.c_str());	
		} else if (it->first.compare("cname")==0) {
			//Check if already got one
			if (cname)
				//Delete it
				free(cname);
			//Clone
			cname = strdup(it->second.c_str());
		} else if (it->first.compare("useNACK")==0) {
			//Set NACK feedback
			useNACK = atoi(it->second.c_str());
			//Enable NACK until first RTT
			isNACKEnabled = useNACK;
		} else if (it->first.compare("usePLI")==0) {
			//Set rtx
			usePLI = atoi(it->second.c_str());
		} else if (it->first.compare("useRTX")==0) {
			//Set rtx
			useRTX = atoi(it->second.c_str());
		} else if (it->first.compare("rtx.apt")==0) {
			//Set apt
			apt = atoi(it->second.c_str());
		} else if (it->first.compare("urn:ietf:params:rtp-hdrext:ssrc-audio-level")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPHeaderExtension::SSRCAudioLevel;
		} else if (it->first.compare("urn:ietf:params:rtp-hdrext:toffset")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPHeaderExtension::TimeOffset;
		} else if (it->first.compare("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPHeaderExtension::AbsoluteSendTime;
			//Use timestamsp
			useAbsTime = true;
		} else {
			Error("-RTPSession::SetProperties() | Unknown RTP property [%s]\n",it->first.c_str());
		}
	}

	return 1;
}

int RTPSession::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	//Set it on transport
	return transport.SetLocalSTUNCredentials(username,pwd);
}


int RTPSession::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	//Set it on transport
	return transport.SetRemoteSTUNCredentials(username,pwd);
}

int RTPSession::SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint)
{
	//Set it on transport
	return transport.SetRemoteCryptoDTLS(setup,hash,fingerprint);
}

int RTPSession::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	//Set it on transport
	return transport.SetRemoteCryptoSDES(suite,key64);
}

void RTPSession::SetReceivingRTPMap(RTPMap &map)
{
	//If we already have one
	if (rtpMapIn)
		//Delete it
		delete(rtpMapIn);
	//Clone it
	rtpMapIn = new RTPMap(map);
}

int RTPSession::SetLocalPort(int recvPort)
{
	//Set it on transport
	return transport.SetLocalPort(recvPort);
}

int RTPSession::GetLocalPort()
{
	//Get it from transport
	return transport.GetLocalPort();
}

bool RTPSession::SetSendingCodec(DWORD codec)
{
	//Check rtp map
	if (!rtpMapOut)
		//Error
		return Error("-RTPSession::SetSendingCodec(%s) | error: no out RTP map\n",MediaFrame::TypeToString(media));

	//Try to find it in the map
	DWORD type = rtpMapOut->GetTypeForCodec(codec);
	
	//If not found
	if (type==RTPMap::NotFound)
		//Not found
		return Error("-RTPSession::SetSendingCodec(%s) | error: codec mapping not found [codec:%s]\n",MediaFrame::TypeToString(media),GetNameForCodec(media,codec));
	//Log it
	Log("-RTPSession::SetSendingCodec(%s) | [codec:%s,type:%d]\n",MediaFrame::TypeToString(media),GetNameForCodec(media,codec),type);
	//Set type
	sendType = type;
	//and we are done
	return true;
}

/***********************************
* SetRemotePort
*	Inicia la sesion rtp de video remota
***********************************/
int RTPSession::SetRemotePort(char *ip,int sendPort)
{
	//Set it on transport
	return transport.SetRemotePort(ip,sendPort);
}

void RTPSession::SetRemoteRateEstimator(RemoteRateEstimator* estimator)
{
	//Store it
	remoteRateEstimator = estimator;

	//Add as listener
	remoteRateEstimator->SetListener(this);
}

/********************************
* Init
*	Inicia el control rtcp
********************************/
int RTPSession::Init()
{
	//Start transport
	return transport.Init();
}

/*********************************
* End
*	Termina la todo
*********************************/
int RTPSession::End()
{
	//End transport
	if (!transport.End())
		//Error
		return 0;
	
	//Check listener
	if (remoteRateEstimator && recv.media.ssrc)
		//Remove stream
		remoteRateEstimator->RemoveStream(recv.media.ssrc);

	//OK
	return 1;
}

int RTPSession::SendPacket(RTCPCompoundPacket &rtcp)
{
	BYTE data[RTPPAYLOADSIZE+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	DWORD size = RTPPAYLOADSIZE;
	int ret = 0;

	//Lock muthed inside  method
	ScopedLock method(sendMutex);

	//Serialize
	int len = rtcp.Serialize(data,size);
	//Check result
	if (len<=0 || len>size)
		//Error
		return Error("-RTPSession::SendPacket(%s) | Error serializing RTCP packet [len:%d]\n",MediaFrame::TypeToString(media),len);

	//Send it
	len = transport.SendRTCPPacket(data,len);

	//Check error
	if (len<0)
		//Return
		return Error("-RTPSession::SendPacket(%s) | Error sending RTCP packet [%d]\n",MediaFrame::TypeToString(media),errno);

	//INcrease stats
	send.media.numPackets++;
	send.media.totalRTCPBytes += len;
	//Exit
	return 1;
}

int RTPSession::SendPacket(RTPPacket &packet)
{
	return SendPacket(packet,packet.GetTimestamp());
}

int RTPSession::SendPacket(RTPPacket &packet,DWORD timestamp)
{
	BYTE  data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD size = MTU;
	
	//Check if we need to send SR (1 per second
	if (useRTCP && (!send.media.lastSenderReport || getTimeDiff(send.media.lastSenderReport)>1E6))
		//Send it
		SendSenderReport();
	
	//Calculate last timestamp
	send.media.lastTime = send.media.time + timestamp;

	//Modificamos las cabeceras del packete
	RTPHeader header;
	RTPHeaderExtension extension;
	
	//Init send packet
	header.ssrc		= send.media.ssrc;
	header.timestamp	= send.media.lastTime;
	header.payloadType	= sendType;
	header.sequenceNumber	= send.media.extSeq++;
	header.mark		= packet.GetMark();

	//Check seq wrap
	if (send.media.extSeq==0)
		//Inc cycles
		send.media.cycles++;

	//If we have are using any sending extensions
	if (useAbsTime)
	{
		//Use extension
		header.extension = true;
		//Set abs send time
		extension.hasAbsSentTime = true;
		extension.absSentTime = getTimeMS();
	}

	//Serialize header
	int len = header.Serialize(data,size);

	//Check
	if (len)
		//Error
		return Error("-RTPSession::SendPacket() | Error serializing rtp headers\n");

	//If we have extension
	if (header.extension)
	{
		//Serialize
		int n = extension.Serialize(extMap,data+len,size-len);
		//Comprobamos que quepan
		if (!n)
			//Error
			return Error("-RTPSession::SendPacket() | Error serializing rtp extension headers\n");
		//Inc len
		len += n;
	}

	//Ensure we have enougth data
	if (len+packet.GetMediaLength()>size)
		//Error
		return Error("-RTPSession::SendPacket() | Media overflow\n");

	//Copiamos los datos
	memcpy(data+len,packet.GetMediaData(),packet.GetMediaLength());

	//Set pateckt length
	len += packet.GetMediaLength();

	//Block
	sendMutex.Lock();

	//Add it rtx queue before encripting
	if (useNACK)
	{
		//Create new packet with this data
		RTPPacket *rtx = new RTPPacket(packet.GetMedia(),packet.GetCodec(),header,extension);
		//Set media
		rtx->SetPayload(packet.GetMediaData(),packet.GetMediaLength());
		//Add it to que
		rtxs[packet.GetExtSeqNum()] = rtx;
	}
	

	//No error yet, send packet
	len = transport.SendRTPPacket(data,len);

	//If got packet to send
	if (len>0)
	{
		//Inc stats
		send.media.numPackets++;
		send.media.totalBytes += len;
	}

	//Get time for packets to discard, always have at least 200ms, max 500ms
	QWORD until = getTimeMS() - (200+fmin(rtt*2,300));
	//Delete old packets
	RTPOrderedPackets::iterator it = rtxs.begin();
	//Until the end
	while(it!=rtxs.end())
	{
		//Get pacekt
		RTPPacket *pkt = it->second;
		//Check time
		if (pkt->GetTime()>until)
			//Keep the rest
			break;
		//DElete from queue and move next
		rtxs.erase(it++);
		//Delete object
		delete(pkt);
	}

	//Unlock
	sendMutex.Unlock();

	//Exit
	return (len>0);
}

void RTPSession::onRTPPacket(BYTE* data, DWORD size)
{
	bool isRTX = false;
	RTPHeader header;
	RTPHeaderExtension extension;
	
	//Check rtp map
	if (!rtpMapIn)
	{
		//Error
		Error("-RTPSession::onRTPPacket(%s) | RTP map not set\n",MediaFrame::TypeToString(media));
		//Exit
		return;
	}
	//Parse RTP header
	int ini = header.Parse(data,size);
	
	//On error
	if (!ini)
	{
		//Debug
		Debug("-RTPSession::onRTPPacket() | Could not parse RTP header\n");
		//Dump it
		Dump(data,size);
		//Exit
		return;
	}
	
	//If it has extension
	if (header.extension)
	{
		//Parse extension
		int l = extension.Parse(extMap,data+ini,size-ini);
		//If not parsed
		if (!l)
		{
			///Debug
			Debug("-RTPSession::onRTPPacket() | Could not parse RTP header extension\n");
			//Dump it
			Dump(data,size);
			//Exit
			return;
		}
		//Inc ini
		ini += l;
	}
	
	//Get header data as shorcut
	DWORD ssrc = header.ssrc;
	BYTE type  = header.payloadType;
	//Get initial codec
	BYTE codec = rtpMapIn->GetCodecForType(header.payloadType);
	
	//Check codec
	if (codec==RTPMap::NotFound)
	{
		//Exit
		Error("-RTPSession::onRTPPacket(%s) | RTP packet type unknown [%d]\n",MediaFrame::TypeToString(media),type);
		//Exit
		return;
	}
	
	//Create normal packet
	RTPPacket *packet = new RTPPacket(media,codec,header,extension);
	
	//Set the payload
	packet->SetPayload(data+ini,size-ini);
	
	//Get sec number
	WORD seq = packet->GetSeqNum();

	//Check if we have a sequence wrap
	if (seq<0x0FFF && (recv.media.extSeq & 0xFFFF)>0xF000)
		//Increase cycles
		recv.media.cycles++;

	//Set cycles
	packet->SetSeqCycles(recv.media.cycles);
	
	//Check if we got a different SSRC
	if (recv.media.ssrc!=ssrc)
	{
		//Check if it is a retransmission
		if (codec!=VideoCodec::RTX)
		{
			//Log
			Log("-RTPSession::onRTPPacket(%s) | New SSRC [new:%x,old:%x]\n",MediaFrame::TypeToString(media),ssrc,recv.media.ssrc);
			//Send SR to old one
			SendSenderReport();
			//Reset packets
			packets.Reset();
			//Reset cycles
			recv.media.cycles = 0;
			//Reset
			recv.media.extSeq = 0;
			//Update ssrc
			recv.media.ssrc = ssrc;
			//If remote estimator
			if (remoteRateEstimator)
				//Add stream
				remoteRateEstimator->AddStream(recv.media.ssrc);
		} else {
			//IT is a retransmission check if we didn't had the ssrc
			if (!recv.rtx.ssrc)
			{
				//Store it
				recv.rtx.ssrc = ssrc;
				//Log
				Log("-RTPSession::onRTPPacket(%s) | Gor RTX for SSRC [rtx:%x,ssrc:%x]\n",MediaFrame::TypeToString(media),recv.rtx.ssrc,recv.media.ssrc);
			}	
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
			
			UltraDebug("RTX: Got   %.d:RTX for #%d ts:%u\n",type,osn,packet->GetTimestamp());
			
			//Get the associated type
			type = apt;
			//Find codec for type
			codec = rtpMapIn->GetCodecForType(type);
			//Check codec
			if (codec==RTPMap::NotFound)
			{
				 //Error
				 Error("-RTPSession::onRTPPacket(%s) | RTP RTX packet apt type unknown [%d]\n",MediaFrame::TypeToString(media),type);
				 //Exi
				 return;
			}
			
			 //Set original seq num
			packet->SetSeqNum(osn);
			//Set original ssrc
			packet->SetSSRC(recv.media.ssrc);
			//Set original type
			packet->SetType(type);
			//Set codec
			packet->SetCodec(codec);

			//Skip osn from payload
			packet->SkipPayload(2);
			//It is a retrasmision
			isRTX = true;
		}	
	} else {
		//HACK: https://code.google.com/p/webrtc/issues/detail?id=3948
		apt = type;
	}

	//if (media==MediaFrame::Video && !isRTX && seq % 40 ==0)
	//	return Error("RTX: Drop %d %s packet #%d ts:%u\n",type,VideoCodec::GetNameFor((VideoCodec::Type)codec),packet->GetSeqNum(),packet->GetTimestamp());
		
	//Update lost packets
	int lost = losts.AddPacket(packet);

	if (lost) UltraDebug("RTX: Missing %d [media:%s,nack:%d,diff:%llu,rtt:%llu]\n",lost,MediaFrame::TypeToString(media),isNACKEnabled,getDifTime(&lastFPU),rtt);
	
	//If nack is enable t waiting for a PLI/FIR response (to not oeverflow)
	if (isNACKEnabled && getDifTime(&lastFPU)/1000>rtt/2 && lost)
	{
		//Get nacks for lost
		std::list<RTCPRTPFeedback::NACKField*> nacks = losts.GetNacks();

		//Create rtcp sender retpor
		RTCPCompoundPacket* rtcp = CreateSenderReport();
		
		//Create NACK
		RTCPRTPFeedback *nack = RTCPRTPFeedback::Create(RTCPRTPFeedback::NACK,send.media.ssrc,recv.media.ssrc);
		
		//Add 
		for (std::list<RTCPRTPFeedback::NACKField*>::iterator it = nacks.begin(); it!=nacks.end(); ++it)
			//Add it
			nack->AddField(*it);
		
		//Add to packet
		rtcp->AddRTCPacket(nack);
		
		//Send packet
		SendPacket(*rtcp);

		//Delete it
		delete(rtcp);
	}


	//If it is not a retransmission
	if (!isRTX)
	{
		//If remote estimator
		if (remoteRateEstimator)
			//Update rate control
			remoteRateEstimator->Update(recv.media.ssrc,packet,size);

		//Increase stats
		recv.media.numPackets++;
		recv.media.totalBytes += size;
		recv.media.totalBytesSinceLastSR += size;

		//Get ext seq
		DWORD extSeq = packet->GetExtSeqNum();

		//Check if it is the min for this SR
		if (extSeq<recv.media.minExtSeqNumSinceLastSR)
			//Store minimum
			recv.media.minExtSeqNumSinceLastSR = extSeq;

		//If we have a not out of order pacekt
		if (extSeq>recv.media.extSeq)
		{
			//Check possible lost pacekts
			if (recv.media.extSeq && extSeq>(recv.media.extSeq+1))
			{
				//Get number of lost packets
				WORD lost = extSeq-recv.media.extSeq-1;
				//Base packet missing
				WORD base = recv.media.extSeq+1;

				//If remote estimator
				if (remoteRateEstimator)
					//Update estimator
					remoteRateEstimator->UpdateLost(recv.media.ssrc,lost);
			}
			
			//Update seq num
			recv.media.extSeq = extSeq;

			//Get diff from previous
			DWORD diff = getUpdDifTime(&recTimeval)/1000;

			//If it is not first one and not from the same frame
			if (recTimestamp && recTimestamp<packet->GetClockTimestamp())
			{
				//Get difference of arravail times
				int d = (packet->GetClockTimestamp()-recTimestamp)-diff;
				//Check negative
				if (d<0)
					//Calc abs
					d = -d;
				//Calculate variance
				int v = d-recv.media.jitter;
				//Calculate jitter
				recv.media.jitter += v/16;
			}

			//Update rtp timestamp
			recTimestamp = packet->GetClockTimestamp();
		}
	}
	
	//Append packet
	packets.Add(packet);

	//Check if we need to send SR (1 per second
	if (useRTCP && (!send.media.lastSenderReport || getTimeDiff(send.media.lastSenderReport)>1E6))
		//Send it
		SendSenderReport();

	//OK
	return;
}

RTPPacket* RTPSession::GetPacket()
{
	//Wait for pacekts
	return packets.Wait();
}

void RTPSession::CancelGetPacket()
{
	//cancel
	packets.Cancel();
}

void RTPSession::onRemotePeer(const char* ip, const short port)
{
	Log("-RTPSession::onRemotePeer(%s) [%s:%u]\n",MediaFrame::TypeToString(media),ip,port);
	
	if (listener)
		//Request FPU
		listener->onFPURequested(this);
}

void RTPSession::onRTCPPacket(BYTE* buffer, DWORD size)
{
	//Parse it
	RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(buffer,size);
	
	//Check packet
	if (!rtcp)
		//Error
		return;
	
	//Increase stats
	recv.media.numRTCPPackets++;
	recv.media.totalRTCPBytes += rtcp->GetSize();

	//For each packet
	for (int i = 0; i<rtcp->GetPacketCount();i++)
	{
		//Get pacekt
		const RTCPPacket* packet = rtcp->GetPacket(i);
		//Check packet type
		switch (packet->GetType())
		{
			case RTCPPacket::SenderReport:
			{
				const RTCPSenderReport* sr = (const RTCPSenderReport*)packet;
				
				//Store info
				recv.media.lastReceivedSenderNTPTimestamp = sr->GetNTPTimestamp();
				recv.media.lastReceivedSenderReport = getTime();
				
				//Check recievd report
				for (int j=0;j<sr->GetCount();j++)
				{
					//Get report
					RTCPReport *report = sr->GetReport(j);
					//Check ssrc
					if (report->GetSSRC()==send.media.ssrc)
					{
						//Calculate RTT
						if (send.media.lastSenderReport && send.media.IsLastSenderReportNTP(report->GetLastSR()))
						{
							//Calculate new rtt in ms
							DWORD rtt = getTimeDiff(send.media.lastSenderReport)/1000-report->GetDelaySinceLastSRMilis();
							//Set it
							SetRTT(rtt);
						}
					}
				}
				break;
			}
			case RTCPPacket::ReceiverReport:
			{
				const RTCPReceiverReport* rr = (const RTCPReceiverReport*)packet;
				//Check recievd report
				for (int j=0;j<rr->GetCount();j++)
				{
					//Get report
					RTCPReport *report = rr->GetReport(j);
					//Check ssrc
					if (report->GetSSRC()==send.media.ssrc)
					{
						//Calculate RTT
						if (send.media.lastSenderReport && send.media.IsLastSenderReportNTP(report->GetLastSR()))
						{
							//Calculate new rtt in ms
							DWORD rtt = getTimeDiff(send.media.lastSenderReport)/1000-report->GetDelaySinceLastSRMilis();
							//Set it
							SetRTT(rtt);
						}
					}
				}
				break;
			}
				break;
			case RTCPPacket::SDES:
				break;
			case RTCPPacket::Bye:
				break;
			case RTCPPacket::App:
				break;
			case RTCPPacket::RTPFeedback:
			{
				//Get feedback packet
				const RTCPRTPFeedback *fb = (const RTCPRTPFeedback*) packet;
				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPRTPFeedback::NACK:
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							const RTCPRTPFeedback::NACKField *field = (const RTCPRTPFeedback::NACKField*) fb->GetField(i);
							//Resent it
							ReSendPacket(field->pid);
							//Check each bit of the mask
							for (int i=0;i<16;i++)
								//Check it bit is present to rtx the packets
								if ((field->blp >> i) & 1)
									//Resent it
									ReSendPacket(field->pid+i+1);
						}
						break;
					case RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest:
						//Debug("-TempMaxMediaStreamBitrateRequest\n");
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							const RTCPRTPFeedback::TempMaxMediaStreamBitrateField *field = (const RTCPRTPFeedback::TempMaxMediaStreamBitrateField*) fb->GetField(i);
							//Check if it is for us
							if (listener && field->GetSSRC()==send.media.ssrc)
								//call listener
								listener->onTempMaxMediaStreamBitrateRequest(this,field->GetBitrate(),field->GetOverhead());
						}
						break;
					case RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification:
						//Debug("-RTPSession::ProcessRTCPPacket() | TempMaxMediaStreamBitrateNotification\n");
						pendingTMBR = false;
						if (requestFPU)
						{
							requestFPU = false;
							SendFIR();
						}
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							const RTCPRTPFeedback::TempMaxMediaStreamBitrateField *field = (const RTCPRTPFeedback::TempMaxMediaStreamBitrateField*) fb->GetField(i);
						}

						break;
				}
				break;
			}
			case RTCPPacket::PayloadFeedback:
			{
				//Get feedback packet
				const RTCPPayloadFeedback *fb = (const RTCPPayloadFeedback*) packet;
				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPPayloadFeedback::PictureLossIndication:
					case RTCPPayloadFeedback::FullIntraRequest:
						Debug("-RTPSession::ProcessRTCPPacket() | FPU requested\n");
						//Chec listener
						if (listener)
							//Send intra refresh
							listener->onFPURequested(this);
						break;
					case RTCPPayloadFeedback::SliceLossIndication:
						Debug("-RTPSession::ProcessRTCPPacket() | SliceLossIndication\n");
						break;
					case RTCPPayloadFeedback::ReferencePictureSelectionIndication:
						Debug("-RTPSession::ProcessRTCPPacket() | ReferencePictureSelectionIndication\n");
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffRequest:
						Debug("-RTPSession::ProcessRTCPPacket() | TemporalSpatialTradeOffRequest\n");
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffNotification:
						Debug("-RTPSession::ProcessRTCPPacket() | TemporalSpatialTradeOffNotification\n");
						break;
					case RTCPPayloadFeedback::VideoBackChannelMessage:
						Debug("-RTPSession::ProcessRTCPPacket() | VideoBackChannelMessage\n");
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
								//GEt exponent
								BYTE exp = payload[5] >> 2;
								DWORD mantisa = payload[5] & 0x03;
								mantisa = mantisa << 8 | payload[6];
								mantisa = mantisa << 8 | payload[7];
								//Get bitrate
								DWORD bitrate = mantisa << exp;
								//Check if it is for us
								if (listener)
									//call listener
									listener->onReceiverEstimatedMaxBitrate(this,bitrate);
							}
						}
						break;
				}
				break;
			}
			case RTCPPacket::FullIntraRequest:
				//This is message deprecated and just for H261, but just in case
				//Check listener
				if (listener)
					//Send intra refresh
					listener->onFPURequested(this);
				break;
			case RTCPPacket::NACK:
				Log("-RTPSession::ProcessRTCPPacket() | NACK!\n");
				break;
		}
	}
	
	//Delete it
	delete(rtcp);
}

RTCPCompoundPacket* RTPSession::CreateSenderReport()
{
	timeval tv;

	//Create packet
	RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();

	//Get now
	QWORD now = getTime();

	//Create sender report for normal stream
	RTCPSenderReport* sr = send.media.CreateSenderReport(now);

	//Create report
	RTCPReport *report = recv.media.CreateReport(now);

	//If got anything
	if (report)
		//Append it
		sr->AddReport(report);

	//Append SR to rtcp
	rtcp->AddRTCPacket(sr);

	//TODO: 
	//Create sender report for RTX stream
	//RTCPSenderReport* rtx = send.rtx.CreateSenderReport(&tv);
	//Append SR to rtcp
	//rtcp->AddRTCPacket(rtx);

	//Create SDES
	RTCPSDES* sdes = new RTCPSDES();

	//Create description
	RTCPSDES::Description *desc = new RTCPSDES::Description();
	//Set ssrc
	desc->SetSSRC(send.media.ssrc);
	//Add item
	desc->AddItem( new RTCPSDES::Item(RTCPSDES::Item::CName,cname));

	//Add it
	sdes->AddDescription(desc);

	//Add to rtcp
	rtcp->AddRTCPacket(sdes);

	//Return it
	return rtcp;
}

int RTPSession::SendSenderReport()
{
	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	if (remoteRateEstimator)
	{
		//Get lastest estimation and convert to kbps
		DWORD estimation = remoteRateEstimator->GetEstimatedBitrate();
		//If it was ok
		if (estimation)
		{
			//Resend TMMBR
			RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest,send.media.ssrc,recv.media.ssrc);
			//Limit incoming bitrate
			rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(recv.media.ssrc,estimation,0));
			//Add to packet
			rtcp->AddRTCPacket(rfb);
			std::list<DWORD> ssrcs;
			//Get ssrcs
			remoteRateEstimator->GetSSRCs(ssrcs);
			//Create feedback
			// SSRC of media source (32 bits):  Always 0; this is the same convention as in [RFC5104] section 4.2.2.2 (TMMBN).
			RTCPPayloadFeedback *remb = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::ApplicationLayerFeeedbackMessage,send.media.ssrc,0);
			//Send estimation
			remb->AddField(RTCPPayloadFeedback::ApplicationLayerFeeedbackField::CreateReceiverEstimatedMaxBitrate(ssrcs,estimation));
			//Add to packet
			rtcp->AddRTCPacket(remb);
		}
	}

	//Send packet
	int ret =  SendPacket(*rtcp);

	//Delete it
	delete(rtcp);

	//Exit
	return ret;
}

int RTPSession::SendFIR()
{
	Debug("-RTPSession::SendFIR()\n");

	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	//Check mode
	if (!usePLI) 
	{
		//Create fir request
		RTCPPayloadFeedback *fir = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::FullIntraRequest,send.media.ssrc,recv.media.ssrc);
		//ADD field
		fir->AddField(new RTCPPayloadFeedback::FullIntraRequestField(recv.media.ssrc,firReqNum++));
		//Add to rtcp
		rtcp->AddRTCPacket(fir);
	} else {
		//Add PLI
		RTCPPayloadFeedback *pli = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::PictureLossIndication,send.media.ssrc,recv.media.ssrc);
		//Add to rtcp
		rtcp->AddRTCPacket(pli);
	}

	//Send packet
	int ret = SendPacket(*rtcp);

	//Delete it
	delete(rtcp);

	//Exit
	return ret;
}

int RTPSession::RequestFPU()
{
	Debug("-RTPSession::RequestFPU()\n");
	//Drop all paquets queued, we could also hurry up
	packets.Reset();
	//Reset packet lost
	losts.Reset();
	//Do not wait until next RTCP SR
	packets.SetMaxWaitTime(0);
	//Disable NACK
	isNACKEnabled = false;
	//request FIR
	SendFIR();
	//Update last request FPU
	getUpdDifTime(&lastFPU);

	//packets.Reset();
	/*if (!pendingTMBR)
	{
		//request FIR
		SendFIR();
	} else {
		//Wait for TMBN response to no overflow
		requestFPU = true;
	}*/
}

void RTPSession::SetRTT(DWORD rtt)
{
	//Set it
	this->rtt = rtt;
	//if got estimator
	if (remoteRateEstimator)
		//Update estimator
		remoteRateEstimator->UpdateRTT(recv.media.ssrc,rtt);

	//Check RTT to enable NACK
	if (useNACK && rtt < 240)
	{
		//Enable NACK only if RTT is small
		isNACKEnabled = true;
		//Update jitter buffer size in ms in [60+rtt,300]
		packets.SetMaxWaitTime(fmin(60+rtt,300));
	} else {
		//Disable NACK
		isNACKEnabled = false;
		//Reduce jitter buffer as we don't use NACK
		packets.SetMaxWaitTime(60);
	}
	//Debug
	UltraDebug("-RTPSession::SetRTT(%s) | [%dms,nack:%d]\n",MediaFrame::TypeToString(media),rtt,isNACKEnabled);
}

void RTPSession::onTargetBitrateRequested(DWORD bitrate)
{
	UltraDebug("-RTPSession::onTargetBitrateRequested() | [%d]\n",bitrate);

	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	//Create TMMBR
	RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest,send.media.ssrc,recv.media.ssrc);
	//Limit incoming bitrate
	rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(recv.media.ssrc,bitrate,0));
	//Add to packet
	rtcp->AddRTCPacket(rfb);

	//We have a pending request
	pendingTMBR = true;
	//Store values
	pendingTMBBitrate = bitrate;

	//Send packet
	SendPacket(*rtcp);

	//Delete it
	delete(rtcp);
}

int RTPSession::ReSendPacket(int seq)
{
	//Lock send lock inside the method
	ScopedLock method(sendMutex);

	//Calculate ext seq number
	DWORD ext = ((DWORD)send.media.cycles)<<16 | seq;

	//Find packet to retransmit
	RTPOrderedPackets::iterator it = rtxs.find(ext);

	//If we still have it
	if (it!=rtxs.end())
	{
		//Get packet
		RTPPacket* packet = it->second;

		//Data
		BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
		DWORD size = MTU;
		int len = 0;
		
		//Overrride headers
		RTPHeader		header(packet->GetRTPHeader());
		RTPHeaderExtension	extension(packet->GetRTPHeaderExtension());
		
		//If usint RTX type for retransmission
		if (useRTX) 
		{
			//Update RTX headers
			header.ssrc		= send.rtx.ssrc;
			header.payloadType	= rtpMapOut->GetTypeForCodec(VideoCodec::RTX);
			header.sequenceNumber	= send.rtx.extSeq++;
			//Check seq wrap
			if (send.rtx.extSeq==0)
				//Inc cycles
				send.rtx.cycles++;
			//Increase counters
			send.rtx.numPackets++;
			send.rtx.totalBytes += packet->GetMediaLength()+2;
			//No padding
			header.padding		= 0;

			//If using abs-time
			if (useAbsTime)
			{
				header.extension = true;
				extension.hasAbsSentTime = true;
				extension.absSentTime = getTime();
			}
		}
		
		//Serialize header
		int n = header.Serialize(data,size);

		//Comprobamos que quepan
		if (!n)
			//Error
			return Error("-RTPSession::ReSendPacket() | Error serializing rtp headers\n");
		
		//Inc len
		len += n;
		
		//If we have extension
		if (header.extension)
		{
			//Serialize
			n = extension.Serialize(extMap,data+len,size-len);
			//Comprobamos que quepan
			if (!n)
				//Error
				return Error("-RTPSession::ReSendPacket() | Error serializing rtp extension headers\n");
			//Inc len
			len += n;
		}
		
		//If usint RTX type for retransmission
		if (useRTX) 
		{
			//And set the original seq
			set2(data,len,seq);
			//Move payload start
			len += 2;
		}
		
		//Copiamos los datos
		memcpy(data+len,packet->GetMediaData(),packet->GetMediaLength());
		
		Debug("-RTPSession::ReSendPacket() | %d %d\n",seq,ext);
		
		//Send packet
		transport.SendRTPPacket(data,len);
		
	} else {
		Debug("-RTPSession::ReSendPacket() | %d:%d %d not found first %d sending intra instead\n",send.media.cycles,seq,ext,rtxs.size() ?  rtxs.begin()->first : 0);
		//Check if got listener
		if (listener)
			//Request a I frame
			listener->onFPURequested(this);
		//Empty queue without locking again
		//Delete rtx packets
		for (RTPOrderedPackets::iterator it = rtxs.begin(); it!=rtxs.end();++it)
		{
			//Get pacekt
			RTPPacket *pkt = it->second;
			//Delete object
			delete(pkt);
		}

		//Clear list
		rtxs.clear();
	}
}

int RTPSession::SendTempMaxMediaStreamBitrateNotification(DWORD bitrate,DWORD overhead)
{
	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	//Create TMMBR
	RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification,send.media.ssrc,recv.media.ssrc);
	//Limit incoming bitrate
	rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(send.media.ssrc,bitrate,0));
	//Add to packet
	rtcp->AddRTCPacket(rfb);

	//Send packet
	int ret = SendPacket(*rtcp);

	//Delete it
	delete(rtcp);

	//Exit
	return ret;
}

