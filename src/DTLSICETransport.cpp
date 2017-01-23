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
#include <openssl/ossl_typ.h>
#include "DTLSICETransport.h"
#include "opus/opusdecoder.h"




DTLSICETransport::DTLSICETransport(Sender *sender) : dtls(*this)
{
	//Store sender
	this->sender = sender;
	//No active candidate
	active = NULL;
	//SRTP instances
	sendSRTPSession = NULL;
	recvSRTPSession = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
}

/*************************
* ~RTPTransport
* 	Destructor
**************************/
DTLSICETransport::~DTLSICETransport()
{
	//Reset
	Reset();
	
}
int DTLSICETransport::onData(const ICERemoteCandidate* candidate,BYTE* data,DWORD size)
{
	int len = size;
	
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
		if (!recvSRTPSession)
			return Error("-RTPBundleTransport::onData() | No recvSRTPSession\n");
		//unprotect
		srtp_err_status_t err = srtp_unprotect_rtcp(recvSRTPSession,data,&len);
		//Check error
		if (err!=srtp_err_status_ok)
			return Error("-RTPBundleTransport::onData() | Error unprotecting rtcp packet [%d]\n",err);
		
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(data,len);
	
		//Check packet
		if (!rtcp)
		{
			//Debug
			Debug("-RTPBundleTransport::onData() | RTCP wrong data\n");
			//Dump it
			Dump(data,size);
			//Exit
			return 1;
		}
		
		//rtcp->Dump();
		
		//Call listener
		//listener->onRTCP(this,candidate,rtcp);
		
		//Skip
		return 1;
	}

	//Double check it is an RTP packet
	if (!RTPPacket::IsRTP(data,size))
	{
		//Debug
		Debug("-RTPBundleTransport::onData() | Not RTP data recevied\n");
		//Dump it
		Dump(data,size);
		//Exit
		return 1;
	}

	//Check minimum size for rtp packet
	if (size<12)
	{
		//Debug
		Debug("-RTPBundleTransport::onData() | RTP data not big enought[%d]\n",size);
		//Exit
		return 1;
	}

	srtp_err_status_t err;
	//Check session
	if (!recvSRTPSession)
		return Error("-RTPBundleTransport::onData() | No recvSRTPSession\n");
	//unprotect
	err = srtp_unprotect(recvSRTPSession,data,&len);
	//Check status
	if (err!=srtp_err_status_ok)
		//Error
		return Error("-RTPBundleTransport::onData() | Error unprotecting rtp packet [%d]\n",err);
	//Debug(">decoded [%d]\n",len);
	//Dump(data,64);
	//Get ssrc
	DWORD ssrc = RTPPacket::GetSSRC(data);
	//Get type
	BYTE type = RTPPacket::GetType(data);
	
	//Get initial codec
	BYTE codec = rtpMap.GetCodecForType(type);
	
	//Debug("-Got RTP on sssrc:%u type:%d codec:%d\n",ssrc,type,codec);
	
	//Check codec
	if (codec==RTPMap::NotFound)
		//Exit
		return Error("-RTPBundleTransport::onData() | RTP packet type unknown [%d]\n",type);
		//Exit

	//Get incoming group
	auto it = incoming.find(ssrc);
	
	//If not found
	if (it==incoming.end())
		//error
		return Error("-RTPBundleTransport::onData() | Unknown ssrc [%u]\n",ssrc);
	
	//Get group
	RTPIncomingSourceGroup *group = it->second;
	
	
	bool isRTX = false;
	bool discard = false;
	RTPIncomingSource* source;
	
	//Get incomins source
	if (ssrc==group->media.SSRC)
	{
		//IT is the media source
		source = &group->media;
		
	} else if (ssrc==group->rtx.SSRC) {
		//Ensure that it is a RTX codec
		if (codec!=VideoCodec::RTX)
			//error
			return  Error("-RTPBundleTransport::onData() | No RTX codec on rtx sssrc:%u type:%d codec:%d\n",ssrc,type,codec);
		//IT is the rtx
		source = &group->rtx;
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
		 RTPTimedPacket tmp(group->type,data,size);
		 //Get original sequence number
		 WORD osn = get2(data,tmp.GetRTPHeaderLen());

		 UltraDebug("RTX: Got   %.d:RTX for #%d ts:%u\n",type,osn,tmp.GetTimestamp());

		 //Move origin
		 for (int i=tmp.GetRTPHeaderLen()-1;i>=0;--i)
			 //Move
			 data[i+2] = data[i];
		 //Move ini
		 data+=2;
		 //reduze size
		 size-=2;
		 //Set original seq num
		 set2(data,2,osn);
		 //Set original ssrc
		 set4(data,8,group->media.SSRC);
		 //Get the associated type
		 type = group->rtx.apt;
		 //Find codec for type
		 codec = rtpMap.GetCodecForType(group->type);
		 //Check codec
		 if (codec==RTPMap::NotFound)
			  //Error
			  return Error("-RTPSession::ReadRTP(%s) | RTP RTX packet apt type unknown [%d]\n",MediaFrame::TypeToString(group->type),type);
		 //It is a retrasmision
		 isRTX = true;
	} else if (ssrc==group->fec.SSRC) {
		//Ensure that it is a FEC codec
		if (codec!=VideoCodec::FLEXFEC)
			//error
			return  Error("-RTPBundleTransport::onData() | No FLEXFEC codec on fec sssrc:%u type:%d codec:%d\n",ssrc,type,codec);
		//IT is the rtx
		source = &group->fec;
	} else {
		//error
		return Error("-RTPBundleTransport::onData() | Group does not contain ssrc [%u]\n",ssrc);
	}
	
	//Create normal packet
	RTPTimedPacket *packet = new RTPTimedPacket(group->type,data,len);
	
	//Set codec & type again
	packet->SetType(type);
	packet->SetCodec(codec);

	//Process extensions
	//TODO:!!!
	//packet->ProcessExtensions(extMap);
	
	//Get sec number
	WORD seq = packet->GetSeqNum();

	//Check if we have a sequence wrap
	if (seq<0x0FFF && (source->extSeq & 0xFFFF)>0xF000)
		//Increase cycles
		source->cycles++;
	
	//Set cycles
	packet->SetSeqCycles(source->cycles);
	
	
	//Increase stats
	source->numPackets++;
	source->totalPacketsSinceLastSR++;
	source->totalBytes += size;
	source->totalBytesSinceLastSR += size;

	//TODO: Calculate REMB and jitter
	
	//Append to the FEC decoder
	if (group->fec.SSRC)
	{
		//Check if we need to discard it
		discard = !group->fec.decoder.AddPacket(packet);
		
		//Try to recover
		RTPTimedPacket* recovered = group->fec.decoder.Recover();
		//If we have recovered a pacekt
		while(recovered)
		{
			//Get pacekte type
			BYTE t = recovered->GetType();
			//Map receovered data codec
			BYTE c = rtpMap.GetCodecForType(t);
			//Check codec
			if (c!=RTPMap::NotFound)
				//Set codec
				recovered->SetCodec(c);
			else
				//Set type for passtrhought
				recovered->SetCodec(t);
			//Process extensions
			recovered->ProcessExtensions(extMap);
			//Update lost packets
			group->losts.AddPacket(recovered);
			//Check listener
			if (group->listener)
				//Call listeners
				group->listener->onRTP(group,recovered);
			
			//Try to recover another one (yuhu!)
			recovered = group->fec.decoder.Recover();
		}
	}
	
	//If we don't have to discard it
	if (!discard)
	{
		//Update lost packets
		int lost = group->losts.AddPacket(packet);
		
		//TODO: Send NACK
		/*
		//If nack is enable t waiting for a PLI/FIR response (to not oeverflow)
		if (isNACKEnabled && getDifTime(&lastFPU)/1000>rtt/2 && lost)
		{
			//Inc nacked count
			recv.nackedPacketsSinceLastSR += lost;

			//Get nacks for lost
			std::list<RTCPRTPFeedback::NACKField*> nacks = losts.GetNacks();

			//Create rtcp sender retpor
			RTCPCompoundPacket* rtcp = CreateSenderReport();

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
		 */
		

		//Create rtcp sender retpor
		RTCPCompoundPacket rtcp;

		
		//Add to rtcp
		rtcp.AddRTCPacket( RTCPPayloadFeedback::Create(RTCPPayloadFeedback::PictureLossIndication,0,ssrc));

		//Send packet
		Send(rtcp);

		//packet->Dump();
		//Debug("<decoded [%d,%d,%d]\n",len,packet->GetSize(),packet->GetMediaLength());
		//Check listener
		if (group->listener)
			//Call listeners
			group->listener->onRTP(group,packet);
	}
	//Done
	return 1;
}


ICERemoteCandidate* DTLSICETransport::AddRemoteCandidate(const sockaddr_in addr, bool useCandidate, DWORD priority)
{
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	
	//Create new candidate
	ICERemoteCandidate* candidate = new ICERemoteCandidate(inet_ntoa(addr.sin_addr),ntohs(addr.sin_port),this);
	
	// Needed for DTLS in client mode (otherwise the DTLS "Client Hello" is not sent over the wire)
	DWORD len = dtls.Read(data,MTU);
	//Check it
	if (len>0)
		//Send to bundle transport
		sender->Send(candidate,data,len);
	
	//Add to candidates
	candidates.push_back(candidate);
	
	//Should we set this candidate as the active one
	if (!active  || useCandidate)
		//Send data to this one from now on
		active = candidate;
	
	//Return it
	return candidate;
}

void DTLSICETransport::SetProperties(const Properties& properties)
{
	//Cleant maps
	rtpMap.clear();
	extMap.clear();
	
	//Get audio codecs
	Properties audio;
	properties.GetChildren("audio",audio);

	//TODO: support all
	rtpMap[audio.GetProperty("opus.pt",0)] = AudioCodec::OPUS;
	
	//Get video codecs
	Properties video;
	properties.GetChildren("video",video);
	
	//TODO: support all
	rtpMap[video.GetProperty("vp9.pt",0)] = VideoCodec::VP9;
	rtpMap[video.GetProperty("vp9.rtx",0)] = VideoCodec::RTX;
	rtpMap[video.GetProperty("flexfec.pt",0)] = VideoCodec::FLEXFEC;
	
	//Get extensions headers
	Properties headers;
	properties.GetChildren("headers",video);
	
	//For each extension
	for (Properties::const_iterator it=headers.begin();it!=headers.end();++it)
	{
		//Set extension
		if (it->first.compare("urn:ietf:params:rtp-hdrext:toffset")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPPacket::HeaderExtension::TimeOffset;
		} else if (it->first.compare("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")==0) {
			//Set extension
			extMap[atoi(it->second.c_str())] = RTPPacket::HeaderExtension::AbsoluteSendTime;
		} else {
			Error("-RTPSession::SetProperties() | Unknown RTP property [%s]\n",it->first.c_str());
		}
	}
}
			
void DTLSICETransport::Reset()
{
	Log("-RTPBundleTransport reset\n");

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
	if (sendSRTPSession)
		//Dealoacate
		srtp_dealloc(sendSRTPSession);
	//If secure
	if (recvSRTPSession)
		//Dealoacate
		srtp_dealloc(recvSRTPSession);
	
	sendSRTPSession = NULL;
	recvSRTPSession = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
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
	if (len!=policy.rtp.cipher_key_len)
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
	if (sendSRTPSession)
		//Dealoacate
		srtp_dealloc(sendSRTPSession);

	//Set send SSRTP sesion
	sendSRTPSession = session;

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
	if (len!=policy.rtp.cipher_key_len)
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
	if (recvSRTPSession)
		//Dealoacate
		srtp_dealloc(recvSRTPSession);
	//Set it
	recvSRTPSession = session;

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
	}
}

bool DTLSICETransport::AddOutgoingSourceGroup(RTPOutgoingSourceGroup *group)
{
	//Add it for each group ssrc
	outgoing[group->media.SSRC] = group;
	outgoing[group->fec.SSRC] = group;
	outgoing[group->rtx.SSRC] = group;
	
	return true;
	
}

bool DTLSICETransport::RemoveOutgoingSourceGroup(RTPOutgoingSourceGroup *group)
{
	//Remove it from each ssrc
	outgoing.erase(group->media.SSRC);
	outgoing.erase(group->fec.SSRC);
	outgoing.erase(group->rtx.SSRC);
	
	return true;
}

bool DTLSICETransport::AddIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-AddIncomingSourceGroup [ssrc:%u,fec:%u,rtx:%u]\n",group->media.SSRC,group->fec.SSRC,group->rtx.SSRC);
	
	//It must contain media ssrc
	if (!group->media.SSRC)
		return Error("No media ssrc defining, stream will not be added\n");
		
	//Add it for each group ssrc	
	incoming[group->media.SSRC] = group;
	if (group->fec.SSRC)
		incoming[group->fec.SSRC] = group;
	if (group->rtx.SSRC)
		incoming[group->rtx.SSRC] = group;
	//Done
	return true;
}

bool DTLSICETransport::RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group)
{
	Log("-RemoveIncomingSourceGroup [ssrc:%u,fec:%u,rtx:%u]\n",group->media.SSRC,group->fec.SSRC,group->rtx.SSRC);
	
	//It must contain media ssrc
	if (!group->media.SSRC)
		return Error("No media ssrc defined, stream will not be removed\n");
	
	//Remove it from each ssrc
	incoming.erase(group->media.SSRC);
	if (group->fec.SSRC)
		incoming.erase(group->fec.SSRC);
	if (group->rtx.SSRC)
		incoming.erase(group->rtx.SSRC);
	
	//Done
	return true;
}
void DTLSICETransport::Send(RTCPCompoundPacket &rtcp)
{
	BYTE 	data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD   size = MTU;
	
	//If we don't have an active candidate yet
	if (!active)
		//Error
		return (void) Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");
	if (!sendSRTPSession)
		//Error
		return (void) Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	
	//Serialize
	int len = rtcp.Serialize(data,size);
	
	//Check result
	if (len<=0 || len>size)
		//Error
		return (void)Error("-DTLSICETransport::Send() | Error serializing RTCP packet [len:%d]\n",len);

	
	//Encript
	srtp_err_status_t srtp_err_status = srtp_protect_rtcp(sendSRTPSession,data,&len);
	//Check error
	if (srtp_err_status!=srtp_err_status_ok)
		//Error
		return (void)Error("-DTLSICETransport::Send() | Error protecting RTCP packet [%d]\n",srtp_err_status);

	//No error yet, send packet
	sender->Send(active,data,len);
}


void DTLSICETransport::Send(RTPTimedPacket &packet)
{
	BYTE 	data[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	memset(data,0,MTU);
	
	//If we don't have an active candidate yet
	if (!active)
		//Error
		return (void) Debug("-DTLSICETransport::Send() | We don't have an active candidate yet\n");
	if (!sendSRTPSession)
		//Error
		return (void) Debug("-DTLSICETransport::Send() | We don't have an DTLS setup yet\n");
	//Find outgoing source
	auto it = outgoing.find(packet.GetSSRC());
	
	//If not found
	if (it==outgoing.end())
		//Error
		return (void) Error("-DTLSICETransport::Send() | Outgoind source not registered for ssrc:%u\n",packet.GetSSRC());
	
	//Get outgoing group
	RTPOutgoingSourceGroup* group = it->second;
	//Get outgoing source
	RTPOutgoingSource& source = group->media;
	
	//Get bare metal timestamp
	DWORD timestamp = packet.GetTimestamp();
	
	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)data;

	//Init send packet
	headers->version = RTP_VERSION;
	headers->ssrc = htonl(packet.GetSSRC());
	
	//Set type in header
	headers->pt = rtpMap.GetTypeForCodec(packet.GetCodec());

	//Calculate last timestamp
	source.lastTime = source.time + timestamp;

	//POnemos el timestamp
	headers->ts = htonl(packet.GetTimestamp());

	//Incrementamos el numero de secuencia
	headers->seq = htons(packet.GetSeqNum());

	//Set source seq
	source.extSeq = packet.GetExtSeqNum();

	//Set end mark
	headers->m = packet.GetMark();

	//Calculamos el inicio
	int ini = sizeof(rtp_hdr_t);

	/*
	//Get header
	rtp_hdr_ext_t* ext = (rtp_hdr_ext_t*)(data + ini);
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
	data[ini] = extMap.GetTypeForCodec(RTPPacket::HeaderExtension::AbsoluteSendTime) << 4 | 0x02;
	//Set data
	set3(data,ini+1,abs);
	//Increase ini
	ini+=4;
	*/
	
	//Comprobamos que quepan
	if (ini+packet.GetMediaLength()>MTU)
		return (void)Error("-RTPSession::SendPacket(%s) | Overflow [size:%d,max:%d]\n",MediaFrame::TypeToString(group->type),ini+packet.GetMediaLength(),MTU);

	//Copiamos los datos
        memcpy(data+ini,packet.GetMediaData(),packet.GetMediaLength());

	//Set pateckt length
	int len = packet.GetMediaLength()+ini;

	//Create new pacekt
	RTPTimedPacket *rtx = new RTPTimedPacket(group->type,data,len);
	//Set cycles
	rtx->SetSeqCycles(source.cycles);
	//Add it to que
	group->packets[rtx->GetExtSeqNum()] = rtx;
	//Debug(">encoded [%d,%d,%d]\n",len,packet.GetSize(),packet.GetMediaLength());
	//Dump(data,64);
	
	//Encript
	srtp_err_status_t srtp_err_status = srtp_protect(sendSRTPSession,data,&len);
	//Check error
	if (srtp_err_status!=srtp_err_status_ok)
		//Error
		return (void)Error("-RTPTransport::SendPacket() | Error protecting RTP packet [%d]\n",srtp_err_status);
	//Debug("<encoded [%d]\n",len);
	//No error yet, send packet
	len = sender->Send(active,data,len);

	//If got packet to send
	if (len>0)
	{
		//Inc stats
		source.numPackets++;
		source.totalBytes += len;
	}

	//Get time for packets to discard, always have at least 200ms, max 500ms
	DWORD rtt = 0;
	QWORD until = getTime()/1000 - (200+fmin(rtt*2,300));
	//Delete old packets
	auto it2 = group->packets.begin();
	//Until the end
	while(it2!=group->packets.end())
	{
		//Get pacekt
		RTPTimedPacket *pkt = it2->second;
		//Check time
		if (pkt->GetTime()>until)
			//Keep the rest
			break;
		//DElete from queue and move next
		group->packets.erase(it2++);
		//Delete object
		delete(pkt);
	}

	
}