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
#include <libavutil/base64.h>
#include <openssl/ossl_typ.h>

/*************************
* RTPSession
* 	Constructro
**************************/
RTPSession::RTPSession(MediaFrame::Type media,Listener *listener) : transport(this), losts(640)
{
	//Store listener
	this->listener = listener;
	//And media
	this->media = media;
	//Init values
	sendType = -1;
	useRTCP = true;
	useAbsTime = false;
	sendSR = 0;
	sendSRRev = 0;
	recTimestamp = 0;
	recSR = 0;
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
	setZeroTime(&lastSR);
	setZeroTime(&lastReceivedSR);
	rtt = 0;
	
	//NO FEC
	useFEC = false;
	useNACK = false;
	useAbsTime = false;
	isNACKEnabled = false;
	//Reduce jitter buffer to min
	packets.SetMaxWaitTime(60);
	//Fill with 0
	memset(sendPacket,0,MTU+SRTP_MAX_TRAILER_LEN);
	
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
	sendSR = 0;
	sendSRRev = 0;
	recTimestamp = 0;
	recSR = 0;
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
	setZeroTime(&lastSR);
	setZeroTime(&lastReceivedSR);
	rtt = 0;
	
	//NO FEC
	useFEC = false;
	useNACK = false;
	useAbsTime = false;
	isNACKEnabled = false;
	//Reduce jitter buffer to min
	packets.SetMaxWaitTime(60);
	//Fill with 0
	memset(sendPacket,0,MTU+SRTP_MAX_TRAILER_LEN);
	
	//Reset stream soutces
	send.Reset();
	recv.Reset();
	sendRTX.Reset();
	recv.Reset();
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
		RTPTimedPacket *pkt = it->second;
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
			send.SSRC = atoi(it->second.c_str());
		} else if (it->first.compare("ssrcRTX")==0) {
			//Set ssrc for sending
			sendRTX.SSRC = atoi(it->second.c_str());	
		} else if (it->first.compare("cname")==0) {
			//Check if already got one
			if (cname)
				//Delete it
				free(cname);
			//Clone
			cname = strdup(it->second.c_str());
		} else if (it->first.compare("useFEC")==0) {
			//Set fec decoding
			useFEC = atoi(it->second.c_str());
		} else if (it->first.compare("useNACK")==0) {
			//Set fec decoding
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
			recvRTX.apt = atoi(it->second.c_str());
		} else if (it->first.compare("urn:ietf:params:rtp-hdrext:ssrc-audio-level")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPPacket::HeaderExtension::SSRCAudioLevel;
		} else if (it->first.compare("urn:ietf:params:rtp-hdrext:toffset")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPPacket::HeaderExtension::TimeOffset;
		} else if (it->first.compare("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPPacket::HeaderExtension::AbsoluteSendTime;
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
	//Set type in header
	((rtp_hdr_t *)sendPacket)->pt = type;
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
	if (remoteRateEstimator && recv.SSRC)
		//Remove stream
		remoteRateEstimator->RemoveStream(recv.SSRC);

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
	send.numPackets++;
	send.totalRTCPBytes += len;
	//Exit
	return 1;
}

int RTPSession::SendPacket(RTPPacket &packet)
{
	return SendPacket(packet,packet.GetTimestamp());
}

int RTPSession::SendPacket(RTPPacket &packet,DWORD timestamp)
{
	//Check if we need to send SR
	if (useRTCP && (isZeroTime(&lastSR) || getDifTime(&lastSR)>1000000))
		//Send it
		SendSenderReport();

	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;

	//Init send packet
	headers->version = RTP_VERSION;
	headers->ssrc = htonl(send.SSRC);

	//Calculate last timestamp
	send.lastTime = send.time + timestamp;

	//POnemos el timestamp
	headers->ts=htonl(send.lastTime);

	//Incrementamos el numero de secuencia
	headers->seq=htons(send.extSeq++);

	//Check seq wrap
	if (send.extSeq==0)
		//Inc cycles
		send.cycles++;

	//La marca de fin de frame
	headers->m=packet.GetMark();

	//Calculamos el inicio
	int ini = sizeof(rtp_hdr_t);

	//If we have are using any sending extensions
	if (useAbsTime)
	{
		//Get header
		rtp_hdr_ext_t* ext = (rtp_hdr_ext_t*)(sendPacket + ini);
		//Set extension header
		headers->x = 1;
		//Set magic cookie
		ext->ext_type = htons(0xBEDE);
		//Set total length in 32bits words
		ext->len = htons(1);
		//Increase ini
		ini += sizeof(rtp_hdr_ext_t);
		//Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
		// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
		DWORD abs = ((getTimeMS() << 18) / 1000) & 0x00ffffff;
		//Set header
		sendPacket[ini] = extMap.GetTypeForCodec(RTPPacket::HeaderExtension::AbsoluteSendTime) << 4 | 0x02;
		//Set data
		set3(sendPacket,ini+1,abs);
		//Increase ini
		ini+=4;
	}

	//Comprobamos que quepan
	if (ini+packet.GetMediaLength()>MTU)
		return Error("-RTPSession::SendPacket(%s) | Overflow [size:%d,max:%d]\n",MediaFrame::TypeToString(media),ini+packet.GetMediaLength(),MTU);

	//Copiamos los datos
        memcpy(sendPacket+ini,packet.GetMediaData(),packet.GetMediaLength());

	//Set pateckt length
	int len = packet.GetMediaLength()+ini;

	//Block
	sendMutex.Lock();

	//Add it rtx queue before encripting
	if (useNACK)
	{
		//Create new pacekt
		RTPTimedPacket *rtx = new RTPTimedPacket(media,sendPacket,len);
		//Set cycles
		rtx->SetSeqCycles(send.cycles);
		//Add it to que
		rtxs[rtx->GetExtSeqNum()] = rtx;
	}

	//No error yet, send packet
	len = transport.SendRTPPacket(sendPacket,len);

	//If got packet to send
	if (len>0)
	{
		//Inc stats
		send.numPackets++;
		send.totalBytes += len;
	}

	//Get time for packets to discard, always have at least 200ms, max 500ms
	QWORD until = getTime()/1000 - (200+fmin(rtt*2,300));
	//Delete old packets
	RTPOrderedPackets::iterator it = rtxs.begin();
	//Until the end
	while(it!=rtxs.end())
	{
		//Get pacekt
		RTPTimedPacket *pkt = it->second;
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

void RTPSession::onRTPPacket(BYTE* buffer, DWORD size)
{
	bool isRTX = false;
	
	//Check rtp map
	if (!rtpMapIn)
	{
		//Error
		Error("-RTPSession::ReadRTP(%s) | RTP map not set\n",MediaFrame::TypeToString(media));
		//Exit
		return;
	}

	//Get ssrc
	DWORD ssrc = RTPPacket::GetSSRC(buffer);
	//Get type
	BYTE type = RTPPacket::GetType(buffer);
	//Get initial codec
	BYTE codec = rtpMapIn->GetCodecForType(type);

	//Check codec
	if (codec==RTPMap::NotFound)
	{
		//Exit
		Error("-RTPSession::ReadRTP(%s) | RTP packet type unknown [%d]\n",MediaFrame::TypeToString(media),type);
		//Exit
		return;
	}
	
	//Check if we got a different SSRC
	if (recv.SSRC!=ssrc)
	{
		//Check if it is a retransmission
		if (codec!=VideoCodec::RTX)
		{
			//Log
			Log("-RTPSession::ReadRTP(%s) | New SSRC [new:%x,old:%x]\n",MediaFrame::TypeToString(media),ssrc,recv.SSRC);
			//Send SR to old one
			SendSenderReport();
			//Reset packets
			packets.Reset();
			//Reset cycles
			recv.cycles = 0;
			//Reset
			recv.extSeq = 0;
			//Update ssrc
			recv.SSRC = ssrc;
			//If remote estimator
			if (remoteRateEstimator)
				//Add stream
				remoteRateEstimator->AddStream(recv.SSRC);
		} else {
			//IT is a retransmission check if we didn't had the ssrc
			if (!recvRTX.SSRC)
			{
				//Store it
				recvRTX.SSRC = ssrc;
				//Log
				Log("-RTPSession::ReadRTP(%s) | Gor RTX for SSRC [rtx:%x,ssrc:%x]\n",MediaFrame::TypeToString(media),recvRTX.SSRC,recv.SSRC);
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
			//Create temporal packet
			RTPTimedPacket tmp(media,buffer,size);
			//Get original sequence number
			WORD osn = get2(buffer,tmp.GetRTPHeaderLen());
			
			UltraDebug("RTX: Got   %.d:RTX for #%d ts:%u\n",type,osn,tmp.GetTimestamp());
			
			//Move origin
			for (int i=tmp.GetRTPHeaderLen()-1;i>=0;--i)
				//Move
				buffer[i+2] = buffer[i];
			//Move ini
			buffer+=2;
			//reduze size
			size-=2;
			//Set original seq num
			set2(buffer,2,osn);
			//Set original ssrc
			set4(buffer,8,recv.SSRC);
			//Get the associated type
			type = recvRTX.apt;
			//Find codec for type
			codec = rtpMapIn->GetCodecForType(type);
			//Check codec
			if (codec==RTPMap::NotFound)
			{
				 //Error
				 Error("-RTPSession::ReadRTP(%s) | RTP RTX packet apt type unknown [%d]\n",MediaFrame::TypeToString(media),type);
				 //Exi
				 return;
			}
			//It is a retrasmision
			isRTX = true;
		}	
	} else {
		//HACK: https://code.google.com/p/webrtc/issues/detail?id=3948
		recvRTX.apt = type;
	}

	//Create rtp packet
	RTPTimedPacket *packet = NULL;

	//Peek type
	if (codec==TextCodec::T140RED || codec==VideoCodec::RED)
	{
		//Create redundant type
		RTPRedundantPacket *red = new RTPRedundantPacket(media,buffer,size);
		//Get primary type
		BYTE t = red->GetPrimaryType();
		//Map primary data codec
		BYTE c = rtpMapIn->GetCodecForType(t);
		//Check codec
		if (c==RTPMap::NotFound)
		{
			::Dump(buffer,64);
			red->Dump();
			
			//Delete red packet
			delete(red);
			//Exit
			Error("-RTPSession::ReadRTP(%s) | RTP packet type unknown for primary type of redundant data [%d,rd:%d]\n",MediaFrame::TypeToString(media),t,codec);
			return;
		}
		
		//if (media==MediaFrame::Video && !isRTX) UltraDebug("RTX: Got  %d:%s primary %d:%s packet #%d ts:%u\n",type,VideoCodec::GetNameFor((VideoCodec::Type)codec),t,VideoCodec::GetNameFor((VideoCodec::Type)c),red->GetSeqNum(),red->GetTimestamp());
		
		//Set it
		red->SetPrimaryCodec(c);
		//For each redundant packet
		for (int i=0; i<red->GetRedundantCount();++i)
		{
			//Get redundant type
			BYTE t = red->GetRedundantType(i);
			//Map redundant data codec
			BYTE c = rtpMapIn->GetCodecForType(t);
			//Check codec
			if (c==RTPMap::NotFound)
			{
				//Delete red packet
				delete(red);
				//Exit
				Error("-RTPSession::ReadRTP(%s) | RTP packet type unknown for primary type of secundary data [%d,%d,red:%d]\n",MediaFrame::TypeToString(media),i,t,codec);
				return;
			}
			//Set it
			red->SetRedundantCodec(i,c);
		}
		//Set packet
		packet = red;
	} else {
		//Create normal packet
		packet = new RTPTimedPacket(media,buffer,size);
		//if (media==MediaFrame::Video && !isRTX) UltraDebug("RTX: Got  %d:%s packet #%d ts:%u\n",type,VideoCodec::GetNameFor((VideoCodec::Type)codec),packet->GetSeqNum(),packet->GetTimestamp());
	}

	//Set codec & type again
	packet->SetType(type);
	packet->SetCodec(codec);

	//Process extensions
	packet->ProcessExtensions(extMap);
	
	//Get sec number
	WORD seq = packet->GetSeqNum();

	//Check if we have a sequence wrap
	if (seq<0x0FFF && (recv.extSeq & 0xFFFF)>0xF000)
		//Increase cycles
		recv.cycles++;

	//Set cycles
	packet->SetSeqCycles(recv.cycles);
	
	//if (media==MediaFrame::Video && !isRTX && seq % 40 ==0)
	//	return Error("RTX: Drop %d %s packet #%d ts:%u\n",type,VideoCodec::GetNameFor((VideoCodec::Type)codec),packet->GetSeqNum(),packet->GetTimestamp());
		
	//Update lost packets
	int lost = losts.AddPacket(packet);

	if (lost) UltraDebug("RTX: Missing %d [media:%s,nack:%d,diff:%llu,rtt:%llu]\n",MediaFrame::TypeToString(media),lost,isNACKEnabled,getDifTime(&lastFPU),rtt);
	
	//If nack is enable t waiting for a PLI/FIR response (to not oeverflow)
	if (isNACKEnabled && getDifTime(&lastFPU)/1000>rtt/2 && lost)
	{
		//Inc nacked count
		recv.nackedPacketsSinceLastSR += lost;
		
		//Get nacks for lost
		std::list<RTCPRTPFeedback::NACKField*> nacks = losts.GetNacks();

		//Generate new RTCP NACK report
		RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();
		
		//Create NACK
		RTCPRTPFeedback *nack = RTCPRTPFeedback::Create(RTCPRTPFeedback::NACK,send.SSRC,recv.SSRC);
		
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
			remoteRateEstimator->Update(recv.SSRC,packet);

		//Increase stats
		recv.numPackets++;
		recv.totalPacketsSinceLastSR++;
		recv.totalBytes += size;
		recv.totalBytesSinceLastSR += size;

		//Get ext seq
		DWORD extSeq = packet->GetExtSeqNum();

		//Check if it is the min for this SR
		if (extSeq<recv.minExtSeqNumSinceLastSR)
			//Store minimum
			recv.minExtSeqNumSinceLastSR = extSeq;

		//If we have a not out of order pacekt
		if (extSeq>recv.extSeq)
		{
			//Check possible lost pacekts
			if (recv.extSeq && extSeq>(recv.extSeq+1))
			{
				//Get number of lost packets
				WORD lost = extSeq-recv.extSeq-1;
				//Base packet missing
				WORD base = recv.extSeq+1;

				//If remote estimator
				if (remoteRateEstimator)
					//Update estimator
					remoteRateEstimator->UpdateLost(recv.SSRC,lost);
			}
			
			//Update seq num
			recv.extSeq = extSeq;

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
				int v = d-recv.jitter;
				//Calculate jitter
				recv.jitter += v/16;
			}

			//Update rtp timestamp
			recTimestamp = packet->GetClockTimestamp();
		}
	}

	//Check if we are using fec
	if (useFEC)
	{
		//Append to the FEC decoder
		fec.AddPacket(packet);

		//Try to recover
		RTPTimedPacket* recovered = fec.Recover();
		//If we have recovered a pacekt
		while(recovered)
		{
			//Get pacekte type
			BYTE t = recovered->GetType();
			//Map receovered data codec
			BYTE c = rtpMapIn->GetCodecForType(t);
			//Check codec
			if (c!=RTPMap::NotFound)
				//Set codec
				recovered->SetCodec(c);
			else
				//Set type for passtrhought
				recovered->SetCodec(t);
			//Process extensions
			recovered->ProcessExtensions(extMap);
			//Append recovered packet
			packets.Add(recovered);
			//Try to recover another one (yuhu!)
			recovered = fec.Recover();
		}
	}

	//Append packet
	packets.Add(packet);

	//Check if we need to send SR
	if (useRTCP && (isZeroTime(&lastSR) || getDifTime(&lastSR)>1000000))
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
	recv.numRTCPPackets++;
	recv.totalRTCPBytes += rtcp->GetSize();

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
				//Get Timestamp, the middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) received as part of the most recent RTCP sender report (SR) packet from source SSRC_n. If no SR has been received yet, the field is set to zero.
				recSR = sr->GetNTPTimestamp() >> 16;

				//Uptade last received SR
				getUpdDifTime(&lastReceivedSR);
				//Check recievd report
				for (int j=0;j<sr->GetCount();j++)
				{
					//Get report
					RTCPReport *report = sr->GetReport(j);
					//Check ssrc
					if (report->GetSSRC()==send.SSRC)
					{
						//Calculate RTT
						if (!isZeroTime(&lastSR) && (report->GetLastSR() == sendSR || report->GetLastSR() == sendSRRev) )
						{
							//Calculate new rtt
							DWORD rtt = getDifTime(&lastSR)/1000-report->GetDelaySinceLastSRMilis();
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
					if (report->GetSSRC()==send.SSRC)
					{
						//Calculate RTT
						if (!isZeroTime(&lastSR) && (report->GetLastSR() == sendSR || report->GetLastSR() == sendSRRev))
						{
							//Calculate new rtt
							DWORD rtt = getDifTime(&lastSR)/1000-report->GetDelaySinceLastSRMilis();
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
								if ((field->blp >> (15-i)) & 1)
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
							if (listener && field->GetSSRC()==send.SSRC)
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
	gettimeofday(&tv, NULL);

	//Create sender report for normal stream
	RTCPSenderReport* sr = send.CreateSenderReport(&tv);

	//Update time of latest sr
	DWORD sinceLastSR = getUpdDifTime(&lastSR);

	//If we have received somthing
	if (recv.totalPacketsSinceLastSR && recv.extSeq>=recv.minExtSeqNumSinceLastSR)
	{
		//Get number of total packtes
		DWORD total = recv.extSeq - recv.minExtSeqNumSinceLastSR + 1;
		//Calculate lost
		DWORD lostPacketsSinceLastSR = total - recv.totalPacketsSinceLastSR;
		//Add to total lost count
		recv.lostPackets += lostPacketsSinceLastSR;
		//Calculate fraction lost
		DWORD frac = (lostPacketsSinceLastSR*256)/total;

		//Create report
		RTCPReport *report = new RTCPReport();

		//Set SSRC of incoming rtp stream
		report->SetSSRC(recv.SSRC);

		//Check last report time
		if (!isZeroTime(&lastReceivedSR))
			//Get time and update it
			report->SetDelaySinceLastSRMilis(getDifTime(&lastReceivedSR)/1000);
		else
			//No previous SR
			report->SetDelaySinceLastSRMilis(0);
		//Set data
		report->SetLastSR(recSR);
		report->SetFractionLost(frac);
		report->SetLastJitter(recv.jitter);
		report->SetLostCount(recv.lostPackets);
		report->SetLastSeqNum(recv.extSeq);

		//Reset data
		recv.totalPacketsSinceLastSR = 0;
		recv.totalBytesSinceLastSR = 0;
		recv.nackedPacketsSinceLastSR = 0;
		recv.minExtSeqNumSinceLastSR = RTPPacket::MaxExtSeqNum;

		//Append it
		sr->AddReport(report);
	}

	//Append SR to rtcp
	rtcp->AddRTCPacket(sr);

	//TODO: 
	//Create sender report for RTX stream
	//RTCPSenderReport* rtx = sendRTX.CreateSenderReport(&tv);
	//Append SR to rtcp
	//rtcp->AddRTCPacket(rtx);

	//Store last send SR 32 middle bits
	sendSR = sr->GetNTPSec() << 16 | sr->GetNTPFrac() >> 16;
	//Store last 16bits of each word to match cisco bug
	sendSRRev = sr->GetNTPSec() << 16 | (sr->GetNTPFrac() | 0xFFFF);

	//Create SDES
	RTCPSDES* sdes = new RTCPSDES();

	//Create description
	RTCPSDES::Description *desc = new RTCPSDES::Description();
	//Set ssrc
	desc->SetSSRC(send.SSRC);
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
			RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest,send.SSRC,recv.SSRC);
			//Limit incoming bitrate
			rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(recv.SSRC,estimation,0));
			//Add to packet
			rtcp->AddRTCPacket(rfb);
			std::list<DWORD> ssrcs;
			//Get ssrcs
			remoteRateEstimator->GetSSRCs(ssrcs);
			//Create feedback
			// SSRC of media source (32 bits):  Always 0; this is the same convention as in [RFC5104] section 4.2.2.2 (TMMBN).
			RTCPPayloadFeedback *remb = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::ApplicationLayerFeeedbackMessage,send.SSRC,0);
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
		RTCPPayloadFeedback *fir = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::FullIntraRequest,send.SSRC,recv.SSRC);
		//ADD field
		fir->AddField(new RTCPPayloadFeedback::FullIntraRequestField(recv.SSRC,firReqNum++));
		//Add to rtcp
		rtcp->AddRTCPacket(fir);
	} else {
		//Add PLI
		RTCPPayloadFeedback *pli = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::PictureLossIndication,send.SSRC,recv.SSRC);
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
		remoteRateEstimator->UpdateRTT(recv.SSRC,rtt);

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
	RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest,send.SSRC,recv.SSRC);
	//Limit incoming bitrate
	rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(recv.SSRC,bitrate,0));
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
	DWORD ext = ((DWORD)send.cycles)<<16 | seq;

	//Find packet to retransmit
	RTPOrderedPackets::iterator it = rtxs.find(ext);

	//If we still have it
	if (it!=rtxs.end())
	{
		//Get packet
		RTPTimedPacket* packet = it->second;

		//Data
		BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
		DWORD size = MTU;
		int len = packet->GetSize();
		
		//Check size + osn in case of RTX
		if (len+2>size)
			//Error
			return Error("-RTPSession::ReSendPacket() | not enougth size for copying packet [len:%d]\n",len);
		
		//Copy RTP headers
		memcpy(data,packet->GetData(),packet->GetRTPHeaderLen());
		
		//Get payload ini
		BYTE *payload = data+packet->GetRTPHeaderLen();
		
		//If using abs-time
		if (useAbsTime)
		{
			//Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
			// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
			DWORD abs = ((getTimeMS() << 18) / 1000) & 0x00ffffff;
			//Overwrite it
			set3(data,sizeof(rtp_hdr_t)+sizeof(rtp_hdr_ext_t)+1,abs);
		}
		
		//If usint RTX type for retransmission
		if (useRTX) 
		{
			rtp_hdr_t* headers = (rtp_hdr_t*)data;
			//Set RTX ssrc
			headers->ssrc = htonl(sendRTX.SSRC);
			//Set payload
			headers->pt = rtpMapOut->GetTypeForCodec(VideoCodec::RTX);
			//Incrementamos el numero de secuencia
			headers->seq = htons(sendRTX.extSeq++);
			//Check seq wrap
			if (sendRTX.extSeq==0)
				//Inc cycles
				sendRTX.cycles++;
			//And set the original seq
			set2(payload,0,seq);
			//Move payload start
			payload += 2;
			//Increase packet len
			len += 2;
			//Increase counters
			sendRTX.numPackets++;
			sendRTX.totalBytes += packet->GetMediaLength()+2;
		}
		
		//Copy payload
		memcpy(payload,packet->GetMediaData(),packet->GetMediaLength());
		
		Debug("-RTPSession::ReSendPacket() | %d %d\n",seq,ext);
		
		//Send packet
		transport.SendRTPPacket(data,len);
		
	} else {
		Debug("-RTPSession::ReSendPacket() | %d:%d %d not found first %d sending intra instead\n",send.cycles,seq,ext,rtxs.size() ?  rtxs.begin()->first : 0);
		//Check if got listener
		if (listener)
			//Request a I frame
			listener->onFPURequested(this);
		//Empty queue without locking again
		//Delete rtx packets
		for (RTPOrderedPackets::iterator it = rtxs.begin(); it!=rtxs.end();++it)
		{
			//Get pacekt
			RTPTimedPacket *pkt = it->second;
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
	RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification,send.SSRC,recv.SSRC);
	//Limit incoming bitrate
	rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(send.SSRC,bitrate,0));
	//Add to packet
	rtcp->AddRTCPacket(rfb);

	//Send packet
	int ret = SendPacket(*rtcp);

	//Delete it
	delete(rtcp);

	//Exit
	return ret;
}

