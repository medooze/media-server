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

BYTE rtpEmpty[] = {0x80,0x14,0x00,0x00,0x00,0x00,0x00,0x00};

//srtp library initializers
class SRTPLib
{
public:
	SRTPLib()	{ srtp_init();	}
};
SRTPLib srtp;

DWORD RTPSession::minLocalPort = 49152;
DWORD RTPSession::maxLocalPort = 65535;
int RTPSession::minLocalPortRange = 50;

bool RTPSession::SetPortRange(int minPort, int maxPort)
{
	// mitPort should be even
	if ( minPort % 2 )
		minPort++;

	//Check port range is possitive
	if (maxPort<minPort)
		//Error
		return Error("-RTPSession::SetPortRange() | port range invalid [%d,%d]\n",minPort,maxPort);

	//check min range ports
	if (maxPort-minPort<minLocalPortRange)
	{
		//Error
		Error("-RTPSession::SetPortRange() | port range too short %d, should be at least %d\n",maxPort-minPort,minLocalPortRange);
		//Correct
		maxPort = minPort+minLocalPortRange;
	}

	//check min range
	if (minPort<1024)
	{
		//Error
		Error("-RTPSession::SetPortRange() | min rtp port is inside privileged range, increasing it\n");
		//Correct it
		minPort = 1024;
	}

	//Check max port
	if (maxPort>65535)
	{
		//Error
		Error("-RTPSession::SetPortRange() | max rtp port is too high, decreasing it\n");
		//Correc it
		maxPort = 65535;
	}

	//Set range
	minLocalPort = minPort;
	maxLocalPort = maxPort;

	//Log
	Log("-RTPSession::SetPortRange() | configured RTP/RTCP ports range [%d,%d]\n", minLocalPort, maxLocalPort);

	//OK
	return true;
}

/*************************
* RTPSession
* 	Constructro
**************************/
RTPSession::RTPSession(MediaFrame::Type media,Listener *listener) : dtls(*this), losts(640)
{
	//Store listener
	this->listener = listener;
	//And media
	this->media = media;
	//Init values
	sendType = -1;
	simSocket = FD_INVALID;
	simRtcpSocket = FD_INVALID;
	simPort = 0;
	simRtcpPort = 0;
	useRTCP = true;
	useAbsTime = false;
	sendSR = 0;
	sendSRRev = 0;
	recTimestamp = 0;
	recSR = 0;
	setZeroTime(&recTimeval);
	recIP = INADDR_ANY;
	recPort = 0;
	prio = 0;
	firReqNum = 0;
	requestFPU = false;
	pendingTMBR = false;
	pendingTMBBitrate = 0;
	//Don't use PLI by default
	usePLI = false;
	//Not muxing
	muxRTCP = false;
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
	//No cripto
	encript = false;
	decript = false;
	sendSRTPSession = NULL;
	recvSRTPSession = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	//NO FEC
	useFEC = false;
	useNACK = false;
	useAbsTime = false;
	isNACKEnabled = false;
	//Reduce jitter buffer to min
	packets.SetMaxWaitTime(60);
	//Fill with 0
	memset(sendPacket,0,MTU+SRTP_MAX_TRAILER_LEN);
	//Preparamos las direcciones de envio
	memset(&sendAddr,       0,sizeof(struct sockaddr_in));
	memset(&sendRtcpAddr,   0,sizeof(struct sockaddr_in));
	//No thread
	setZeroThread(&thread);
	running = false;
	//No stimator
	remoteRateEstimator = NULL;

	//Set family
	sendAddr.sin_family     = AF_INET;
	sendRtcpAddr.sin_family = AF_INET;
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
	//Delete packets
	packets.Clear();
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
	if (cname)
		free(cname);
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
	recIP = INADDR_ANY;
	recPort = 0;
	firReqNum = 0;
	requestFPU = false;
	pendingTMBR = false;
	pendingTMBBitrate = 0;
	//Not muxing
	muxRTCP = false;
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
	//No cripto
	encript = false;
	decript = false;
	sendSRTPSession = NULL;
	recvSRTPSession = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	//NO FEC
	useFEC = false;
	useNACK = false;
	useAbsTime = false;
	isNACKEnabled = false;
	//Reduce jitter buffer to min
	packets.SetMaxWaitTime(60);
	//Fill with 0
	memset(sendPacket,0,MTU+SRTP_MAX_TRAILER_LEN);
	//Preparamos las direcciones de envio
	memset(&sendAddr,       0,sizeof(struct sockaddr_in));
	memset(&sendRtcpAddr,   0,sizeof(struct sockaddr_in));
	//Set family
	sendAddr.sin_family     = AF_INET;
	sendRtcpAddr.sin_family = AF_INET;
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
	
	Debug("-FlushRTXPackets\n");

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

int RTPSession::SetLocalCryptoSDES(const char* suite,const BYTE* key,const DWORD len)
{
	srtp_err_status_t err;
	srtp_policy_t policy;

	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	//Get cypher
	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		Log("-RTPSession::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-RTPSession::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-RTPSession::SetLocalCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-RTPSession::SetLocalCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else {
		return Error("-RTPSession::SetLocalCryptoSDES() | Unknown cipher suite: %s", suite);
	}

	//Check sizes
	if (len!=policy.rtp.cipher_key_len)
		//Error
		return Error("-RTPSession::SetLocalCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

	//Set polciy values
	policy.ssrc.type	= ssrc_any_outbound;
	policy.ssrc.value	= 0;
	policy.allow_repeat_tx  = 1;
	policy.key		= (BYTE*)key;
	policy.next		= NULL;

	//Create new
	srtp_t session;
	err = srtp_create(&session,&policy);

	//Check error
	if (err!=srtp_err_status_ok)
		//Error
		return Error("-RTPSession::SetLocalCryptoSDES() | Failed to create local SRTP session | err:%d\n", err);
	
	//if we already got a send session don't leak it
	if (sendSRTPSession)
		//Dealoacate
		srtp_dealloc(sendSRTPSession);

	//Set send SSRTP sesion
	sendSRTPSession = session;

	//Request an intra to start clean
	if (listener)
		//Request a I frame
		listener->onFPURequested(this);

	//Evrything ok
	return 1;
}

int RTPSession::SetLocalCryptoSDES(const char* suite, const char* key64)
{
	//Log
	Log("-RTPSession::SetLocalCryptoSDES() | [key:%s,suite:%s]\n",key64,suite);

	//Get lenght
	WORD len64 = strlen(key64);
	//Allocate memory for the key
	BYTE sendKey[len64];
	//Decode
	WORD len = av_base64_decode(sendKey,key64,len64);

	//Set it
	return SetLocalCryptoSDES(suite,sendKey,len);
}

int RTPSession::SetProperties(const Properties& properties)
{
	//Clean extension map
	extMap.clear();

	//For each property
	for (Properties::const_iterator it=properties.begin();it!=properties.end();++it)
	{
		Log("-RTPSession::SetProperties() | Setting RTP property [%s:%s]\n",it->first.c_str(),it->second.c_str());

		//Check
		if (it->first.compare("rtcp-mux")==0)
		{
			//Set rtcp muxing
			muxRTCP = atoi(it->second.c_str());
		} else if (it->first.compare("useRTCP")==0) {
			//Set rtx
			useRTCP = atoi(it->second.c_str());
		} else if (it->first.compare("secure")==0) {
			//Encript and decript
			encript = true;
			decript = true;
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
	Log("-RTPSession::SetLocalSTUNCredentials() | [frag:%s,pwd:%s]\n",username,pwd);
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


int RTPSession::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	Log("-RTPSession::SetRemoteSTUNCredentials() |  [frag:%s,pwd:%s]\n",username,pwd);
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

int RTPSession::SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint)
{
	Log("-RTPSession::SetRemoteCryptoDTLS | [setup:%s,hash:%s,fingerprint:%s]\n",setup,hash,fingerprint);

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
		return Error("-RTPSession::SetRemoteCryptoDTLS | Unknown setup");

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
		return Error("-RTPSession::SetRemoteCryptoDTLS | Unknown hash");

	//Init DTLS
	return dtls.Init();
}

int RTPSession::SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len)
{
	srtp_err_status_t err;
	srtp_policy_t policy;

	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		Log("-RTPSession::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-RTPSession::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-RTPSession::SetRemoteCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-RTPSession::SetRemoteCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else {
		return Error("-RTPSession::SetRemoteCryptoSDES() | Unknown cipher suite %s", suite);
	}

	//Check sizes
	if (len!=policy.rtp.cipher_key_len)
		//Error
		return Error("-RTPSession::SetRemoteCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

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
		return Error("-RTPSession::SetRemoteCryptoSDES() | Failed to create remote SRTP session | err:%d\n", err);
	
	//if we already got a recv session don't leak it
	if (recvSRTPSession)
		//Dealoacate
		srtp_dealloc(recvSRTPSession);
	//Set it
	recvSRTPSession = session;

	//Everything ok
	return 1;
}

int RTPSession::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	//Log
	Log("-RTPSession::SetRemoteCryptoSDES() | [key:%s,suite:%s]\n",key64,suite);

	//Decript
	decript = true;

	//Get length
	WORD len64 = strlen(key64);
	//Allocate memory for the key
	BYTE recvKey[len64];
	//Decode
	WORD len = av_base64_decode(recvKey,key64,len64);

	//Set it
	return SetRemoteCryptoSDES(suite,recvKey,len);
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
	//Override
	simPort = recvPort;
}

int RTPSession::GetLocalPort()
{
	// Return local
	return simPort;
}

bool RTPSession::SetSendingCodec(DWORD codec)
{
	//Check rtp map
	if (!rtpMapOut)
		//Error
		return Error("-RTPSession::SetSendingCodec() | error: no out RTP map\n");

	//Try to find it in the map
	DWORD type = rtpMapOut->GetTypeForCodec(codec);
	
	//If not found
	if (type==RTPMap::NotFound)
		//Not found
		return Error("-RTPSession::SetSendingCodec() | error: codec mapping not found [codec:%s]\n",GetNameForCodec(media,codec));
	//Log it
	Log("-RTPSession::SetSendingCodec() | [codec:%s,type:%d]\n",GetNameForCodec(media,codec),type);
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
	//Get ip addr
	DWORD ipAddr = inet_addr(ip);

	//If we already have one IP binded
	if (recIP!=INADDR_ANY)
		//Exit
		return Log("-RTPSession::SetRemotePort() | NAT already binded sucessfully to [%s:%d]\n",inet_ntoa(sendAddr.sin_addr),recPort);

	//Ok, let's et it
	Log("-RTPSession::SetRemotePort() | [%s:%d]\n",ip,sendPort);

	//Ip y puerto de destino
	sendAddr.sin_addr.s_addr 	= ipAddr;
	sendRtcpAddr.sin_addr.s_addr 	= ipAddr;
	sendAddr.sin_port 		= htons(sendPort);

	//Check if doing rtcp muxing
	if (muxRTCP)
		//Same than rtp
		sendRtcpAddr.sin_port 	= htons(sendPort);
	else
		//One more than rtp
		sendRtcpAddr.sin_port 	= htons(sendPort+1);

	//Open ports
	SendEmptyPacket();

	//Y abrimos los sockets
	return 1;
}

void RTPSession::SendEmptyPacket()
{
	//Open rtp
	sendto(simSocket,rtpEmpty,sizeof(rtpEmpty),0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
	//If not muxing
	if (!muxRTCP)
		//Send
		sendto(simRtcpSocket,rtpEmpty,sizeof(rtpEmpty),0,(sockaddr *)&sendRtcpAddr,sizeof(struct sockaddr_in));
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
	int retries = 0;

	Log(">RTPSession::Init()\n");

	sockaddr_in recAddr;

	//Clear addr
	memset(&recAddr,0,sizeof(struct sockaddr_in));

	//Set family
	recAddr.sin_family     	= AF_INET;

	//Get two consecutive ramdom ports
	while (retries++<100)
	{
		//If we have a rtp socket
		if (simSocket!=FD_INVALID)
		{
			// Close first socket
			MCU_CLOSE(simSocket);
			//No socket
			simSocket = FD_INVALID;
		}
		//If we have a rtcp socket
		if (simRtcpSocket!=FD_INVALID)
		{
			///Close it
			MCU_CLOSE(simRtcpSocket);
			//No socket
			simRtcpSocket = FD_INVALID;
		}

		//Create new sockets
		simSocket = socket(PF_INET,SOCK_DGRAM,0);
		//If not forced to any port
		if (!simPort)
		{
			//Get random
			simPort = (RTPSession::GetMinPort()+(RTPSession::GetMaxPort()-RTPSession::GetMinPort())*double(rand()/double(RAND_MAX)));
			//Make even
			simPort &= 0xFFFFFFFE;
		}
		//Try to bind to port
		recAddr.sin_port = htons(simPort);
		//Bind the rtcp socket
		if(bind(simSocket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
		{
			//Use random
			simPort = 0;
			//Try again
			continue;
		}
		//Create new sockets
		simRtcpSocket = socket(PF_INET,SOCK_DGRAM,0);
		//Next port
		simRtcpPort = simPort+1;
		//Try to bind to port
		recAddr.sin_port = htons(simRtcpPort);
		//Bind the rtcp socket
		if(bind(simRtcpSocket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
		{
			//Use random
			simPort = 0;
			//Try again
			continue;
		}
		//Set COS
		int cos = 5;
		setsockopt(simSocket,     SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
		setsockopt(simRtcpSocket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
		//Set TOS
		int tos = 0x2E;
		setsockopt(simSocket,     IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
		setsockopt(simRtcpSocket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
		//Everything ok
		Log("-RTPSession::Init() | Got ports [%d,%d]\n",simPort,simRtcpPort);
		//Start receiving
		Start();
		//Done
		Log("<RTPSession::Init()\n");
		//Opened
		return 1;
	}

	//Error
	Error("-RTPSession::Init() | too many failed attemps opening sockets\n");

	//Failed
	return 0;
}

/*********************************
* End
*	Termina la todo
*********************************/
int RTPSession::End()
{
	//Check if not running
	if (!running)
		//Nothing
		return 0;

	Log(">RTPSession::End()\n");

	//Stop just in case
	Stop();

	//Not running;
	running = false;
	//If got socket
	if (simSocket!=FD_INVALID)
	{
		//Will cause poll to return
		MCU_CLOSE(simSocket);
		//No sockets
		simSocket = FD_INVALID;
	}
	if (simRtcpSocket!=FD_INVALID)
	{
		//Will cause poll to return
		MCU_CLOSE(simRtcpSocket);
		//No sockets
		simRtcpSocket = FD_INVALID;
	}
	//Check listener
	if (remoteRateEstimator && recv.SSRC)
		//Remove stream
		remoteRateEstimator->RemoveStream(recv.SSRC);

	Log("<RTPSession::End()\n");

	return 1;
}

int RTPSession::SendPacket(RTCPCompoundPacket &rtcp)
{
	BYTE data[RTPPAYLOADSIZE+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	DWORD size = RTPPAYLOADSIZE;
	int ret = 0;

	//Lock muthed inside  method
	ScopedLock method(sendMutex);

	//Check if we have sendinf ip address
	if (sendRtcpAddr.sin_addr.s_addr == INADDR_ANY && !muxRTCP)
	{
		//Debug
		Debug("-RTPSession::SendPacket() | Error sending rtcp packet, no remote IP yet\n");
		//Exit
		return 0;
	}

	//Serialize
	int len = rtcp.Serialize(data,size);
	//Check result
	if (len<=0 || len>size)
		//Error
		return Error("-RTPSession::SendPacket() | Error serializing RTCP packet [len:%d]\n",len);

	//If encripted
	if (encript)
	{
		//Check  session
		if (!sendSRTPSession)
			return Error("-RTPSession::SendPacket() | no sendSRTPSession\n");
		//Protect
		srtp_err_status_t err = srtp_protect_rtcp(sendSRTPSession,data,&len);
		//Check error
		if (err!=srtp_err_status_ok)
			//Nothing
			return Error("-RTPSession::SendPacket() | Error protecting RTCP packet [%d]\n",err);
	}

	//If muxin
	if (muxRTCP)
		//Send using RTP port
		ret = sendto(simSocket,data,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
	else
		//Send using RCTP port
		ret = sendto(simRtcpSocket,data,len,0,(sockaddr *)&sendRtcpAddr,sizeof(struct sockaddr_in));

	//Check error
	if (ret<0)
		//Return
		return Error("-RTPSession::SendPacket() | Error sending RTCP packet [%d]\n",errno);

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
	int ret = 0;

	//Check if we have sendinf ip address
	if (sendAddr.sin_addr.s_addr == INADDR_ANY)
	{
		//Do we have rec ip?
		if (recIP!=INADDR_ANY)
		{
			//Do NAT
			sendAddr.sin_addr.s_addr = recIP;
			//Set port
			sendAddr.sin_port = htons(recPort);
			//Log
			Log("-RTPSession::SendPacket() | NAT: Now sending %s to [%s:%d].\n", MediaFrame::TypeToString(media),inet_ntoa(sendAddr.sin_addr), recPort);
			//Check if using ice
			if (iceRemoteUsername && iceRemotePwd && iceLocalUsername)
			{
				//Create buffer
				BYTE aux[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
				int size = RTPPAYLOADSIZE;

				//Create trans id
				BYTE transId[12];
				//Set first to 0
				set4(transId,0,0);
				//Set timestamp as trans id
				set8(transId,4,getTime());
				//Create binding request to send back
				STUNMessage *request = new STUNMessage(STUNMessage::Request,STUNMessage::Binding,transId);
				//Add username
				request->AddUsernameAttribute(iceLocalUsername,iceRemoteUsername);
				//Add other attributes
				request->AddAttribute(STUNMessage::Attribute::IceControlled,(QWORD)1);
				request->AddAttribute(STUNMessage::Attribute::Priority,(DWORD)33554431);
				//Serialize and autenticate
				int len = request->AuthenticatedFingerPrint(aux,size,iceRemotePwd);
				//Send it
				sendto(simSocket,aux,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

				//Clean response
				delete(request);
			}
		} else {
			//Exit
			Debug("-RTPSession::SendPacket() | No remote address for [%s]\n",MediaFrame::TypeToString(media));
			//Exit
			return 0;
		}
	}

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
		return Error("-RTPSession::SendPacket() | Overflow [size:%d,max:%d]\n",ini+packet.GetMediaLength(),MTU);

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
	int err = 0;

	//Check if we ar encripted
	if (encript)
	{
		//Check  session
		if (sendSRTPSession)
		{
			//Encript
			srtp_err_status_t srtp_err_status = srtp_protect(sendSRTPSession,sendPacket,&len);
			//Check error
			if (srtp_err_status!=srtp_err_status_ok)
			{
				//Error
				Error("-RTPSession::SendPacket() | Error protecting RTP packet [%d]\n",err);
				//Don't send
				err = 1;
			}
		} else {
			//Log
			Debug("-RTPSession::SendPacket() | no sendSRTPSession\n");
			//Don't send
			err = 1;
		}
	}

	//If got packet to send
	if (len && !err)
	{
		//Send packet
		ret = !sendto(simSocket,sendPacket,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
		//Inc stats
		send.numPackets++;
		send.totalBytes += packet.GetMediaLength();
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
	return ret;
}

int RTPSession::ReadRTCP()
{
	BYTE buffer[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Read rtcp socket
	int size = recvfrom(simRtcpSocket,buffer,MTU,MSG_DONTWAIT,(sockaddr*)&from_addr, &from_len);

	// Ignore empty datagrams and errors
	if (size <= 0)
		return 0;

	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(buffer,size))
	{
		//Parse message
		STUNMessage *stun = STUNMessage::Parse(buffer,size);

		//It was not a valid STUN message
		if (! stun)
			//Error
			return Error("-RTPSession::ReadRTCP() | failed to parse STUN message\n");

		//Get type and method
		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();

		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			DWORD len = 0;
			//Create response
			STUNMessage* resp = stun->CreateResponse();
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(&from_addr);
			//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
			//Create  response
			DWORD size = resp->GetSize();
			BYTE *aux = (BYTE*)malloc(size);
			memset(aux, 0, size);

			//Check if we have local passworkd
			if (iceLocalPwd)
				//Serialize and autenticate
				len = resp->AuthenticatedFingerPrint(aux,size,iceLocalPwd);
			else
				//Do nto authenticate
				len = resp->NonAuthenticatedFingerPrint(aux,size);

			//Send it
			sendto(simRtcpSocket,aux,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

			//Clean memory
			free(aux);
			//Clean response
			delete(resp);

			//Do NAT
			sendRtcpAddr.sin_addr.s_addr = from_addr.sin_addr.s_addr;
			//Set port
			sendRtcpAddr.sin_port = from_addr.sin_addr.s_addr;
		}

		//Delete message
		delete(stun);
		//Exit
		return 1;
	}

	//Check if it is RTCP
	if (!RTCPCompoundPacket::IsRTCP(buffer,size))
		//Exit
		return 0;

	//Check if we have sendinf ip address
	if (sendRtcpAddr.sin_addr.s_addr == INADDR_ANY)
	{
		//Do NAT
		sendRtcpAddr.sin_addr.s_addr = from_addr.sin_addr.s_addr;
		//Set port
		sendRtcpAddr.sin_port = from_addr.sin_port;
		//Log it
		Log("-RTPSession::ReadRTCP() | Got first RTCP, sending to %s:%d with rtcp-muxing:%d\n",inet_ntoa(sendRtcpAddr.sin_addr),ntohs(sendRtcpAddr.sin_port),muxRTCP);
	}

	//Decript
	if (decript)
	{
		//Check session
		if (!recvSRTPSession)
			return Error("-RTPSession::ReadRTCP() | No recvSRTPSession\n");
		//unprotect
		srtp_err_status_t err = srtp_unprotect_rtcp(recvSRTPSession,buffer,&size);
		//Check error
		if (err!=srtp_err_status_ok)
			return Error("-RTPSession::ReadRTCP() | Error unprotecting rtcp packet [%d]\n",err);
	}
	//RTCP mux disabled
	muxRTCP = false;
	//Parse it
	RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(buffer,size);
	//Check packet
	if (!rtcp)
		//Error
		return 0;
	//Handle incomming rtcp packets
	ProcessRTCPPacket(rtcp);
	//Delete it
	delete(rtcp);
	//OK
	return 1;
}

/*********************************
* GetTextPacket
*	Lee el siguiente paquete de video
*********************************/
int RTPSession::ReadRTP()
{
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	BYTE *buffer = data;
	sockaddr_in from_addr;
	bool isRTX = false;
	DWORD from_len = sizeof(from_addr);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Leemos del socket
	int size = recvfrom(simSocket,buffer,MTU,MSG_DONTWAIT,(sockaddr*)&from_addr, &from_len);

	// Ignore empty datagrams and errors
	if (size <= 0)
		return 0;

	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(buffer,size))
	{
		//Parse it
		STUNMessage *stun = STUNMessage::Parse(buffer,size);

		//It was not a valid STUN message
		if (!stun)
			//Error
			return Error("-RTPSession::ReadRTP() | failed to parse STUN message\n");

		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();

		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			DWORD len = 0;
			//Create response
			STUNMessage* resp = stun->CreateResponse();
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(&from_addr);
			//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
			//Create  response
			DWORD size = resp->GetSize();
			BYTE *aux = (BYTE*)malloc(size);
			memset(aux, 0, size);

			//Check if we have local passworkd
			if (iceLocalPwd)
				//Serialize and autenticate
				len = resp->AuthenticatedFingerPrint(aux,size,iceLocalPwd);
			else
				//Do nto authenticate
				len = resp->NonAuthenticatedFingerPrint(aux,size);

			//Send it
			sendto(simSocket,aux,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

			//Clean memory
			free(aux);
			//Clean response
			delete(resp);

			//Candidate priority
			DWORD candPrio = 0;

			//Check if it has the prio attribute
			if (stun->HasAttribute(STUNMessage::Attribute::Priority)) 
			{
				//Get attribute
				STUNMessage::Attribute* priority = stun->GetAttribute(STUNMessage::Attribute::Priority);
				//Check size
				if (priority->size==4)
					//Get prio
					candPrio = get4(priority->attr,0);
			}

			//Debug
			Debug("-RTPSession::ReadRTP() | ICE: received bind request from [%s:%d] with candidate [prio:%d,use:%d] current:%d\n", inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port),candPrio,stun->HasAttribute(STUNMessage::Attribute::UseCandidate),prio);
	

			//If use candidate to a differentIP  is set or we don't have another IP address
			if (recIP==INADDR_ANY || 
				(recIP==from_addr.sin_addr.s_addr && sendAddr.sin_addr.s_addr!=from_addr.sin_addr.s_addr) || 
				(stun->HasAttribute(STUNMessage::Attribute::UseCandidate) && candPrio>prio)
			)
			{
				//Check if nominated
				if (stun->HasAttribute(STUNMessage::Attribute::UseCandidate))
					//Update prio
					prio = candPrio;

				//Do NAT
				sendAddr.sin_addr.s_addr = from_addr.sin_addr.s_addr;
				//Set port
				sendAddr.sin_port = from_addr.sin_port;
				//Log
				Log("-RTPSession::ReadRTP() | ICE: Now sending %s to [%s:%d:%d] prio:%d\n", MediaFrame::TypeToString(media),inet_ntoa(sendAddr.sin_addr), ntohs(sendAddr.sin_port),recIP, prio);
				
				//Check if got listener
				if (listener)
					//Request a I frame
					listener->onFPURequested(this);

				DWORD len = 0;
				//Create trans id
				BYTE transId[12];
				//Set first to 0
				set4(transId,0,0);
				//Set timestamp as trans id
				set8(transId,4,getTime());
				//Create binding request to send back
				STUNMessage *request = new STUNMessage(STUNMessage::Request,STUNMessage::Binding,transId);
				//Check usernames
				if (iceLocalUsername && iceRemoteUsername)
					//Add username
					request->AddUsernameAttribute(iceLocalUsername,iceRemoteUsername);
				//Add other attributes
				request->AddAttribute(STUNMessage::Attribute::IceControlled,(QWORD)1);
				request->AddAttribute(STUNMessage::Attribute::Priority,(DWORD)33554431);

				//Create  request
				DWORD size = request->GetSize();
				BYTE* aux = (BYTE*)malloc(size);
				memset(aux, 0, size);

				//Check remote pwd
				if (iceRemotePwd)
					//Serialize and autenticate
					len = request->AuthenticatedFingerPrint(aux,size,iceRemotePwd);
				else
					//Do nto authenticate
					len = request->NonAuthenticatedFingerPrint(aux,size);

				//Send it
				sendto(simSocket,aux,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

				//Clean memory
				free(aux);
				//Clean response
				delete(request);

				// Needed for DTLS in client mode (otherwise the DTLS "Client Hello" is not sent over the wire)
				len = dtls.Read(buffer,MTU);
				//Check it
				if (len>0)
					//Send back
					sendto(simSocket,buffer,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));
			}
		}

		//Delete message
		delete(stun);
		//Exit
		return 1;
	}

	//Check if it is RTCP
	if (RTCPCompoundPacket::IsRTCP(buffer,size))
	{
		//Decript
		if (decript)
		{
			//Check session
			if (!recvSRTPSession)
				return Error("-RTPSession::ReadRTP() | No recvSRTPSession\n");
			//unprotect
			srtp_err_status_t err = srtp_unprotect_rtcp(recvSRTPSession,buffer,&size);
			//Check error
			if (err!=srtp_err_status_ok)
				return Error("-RTPSession::ReadRTP() | Error unprotecting rtcp packet [%d]\n",err);
		}

		//RTCP mux enabled
		muxRTCP = true;
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(buffer,size);
		//Check packet
		if (!rtcp)
			//Error
			return 0;
		
		//Handle incomming rtcp packets
		ProcessRTCPPacket(rtcp);
		//delete it
		delete(rtcp);
		//Skip
		return 1;
	}

	//Check if it a DTLS packet
	if (DTLSConnection::IsDTLS(buffer,size))
	{
		//Feed it
		dtls.Write(buffer,size);

		//Read
		int len = dtls.Read(buffer,MTU);

		//Check it
		if (len>0)
			//Send it back
			sendto(simSocket,buffer,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));
		//Exit
		return 1;
	}

	//Double check it is an RTP packet
	if (!RTPPacket::IsRTP(buffer,size))
	{
		//Debug
		Debug("-RTPSession::ReadRTP() | Not RTP data recevied\n");
		//Dump it
		Dump(buffer,size);
		//Exit
		return 1;
	}

	//If we start receiving from a drifferent ip address or it is the first one
	if (recIP!=from_addr.sin_addr.s_addr)
	{
		//Log
		Log("-RTPSession::ReadRTP() | NAT: received packet from new source [%s:%d]\n", inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));
		//Check if got listener
		if (listener)
			//Request a I frame for start sending
			listener->onFPURequested(this);
	}

	//Get receiving ip address
	recIP = from_addr.sin_addr.s_addr;
	//Get also port
	recPort = ntohs(from_addr.sin_port);
	
	//Check rtp map
	if (!rtpMapIn)
		//Error
		return Error("-RTPSession::ReadRTP() | RTP map not set\n");

	//Check minimum size for rtp packet
	if (size<12)
	{
		//Debug
		Debug("-RTPSession::ReadRTP() | RTP data not big enought[%d]\n",size);
		//Exit
		return 1;
	}

	//Check if it is encripted
	if (decript)
	{
		srtp_err_status_t err;
		//Check session
		if (!recvSRTPSession)
			return Error("-RTPSession::ReadRTP() | No recvSRTPSession\n");
		//unprotect
		err = srtp_unprotect(recvSRTPSession,buffer,&size);
		//Check status
		if (err!=srtp_err_status_ok)
			//Error
			return Error("-RTPSession::ReadRTP() | Error unprotecting rtp packet [%d]\n",err);
	}
	
	//Get ssrc
	DWORD ssrc = RTPPacket::GetSSRC(buffer);
	//Get type
	BYTE type = RTPPacket::GetType(buffer);
	//Get initial codec
	BYTE codec = rtpMapIn->GetCodecForType(type);

	//Check codec
	if (codec==RTPMap::NotFound)
		//Exit
		return Error("-RTPSession::ReadRTP() | RTP packet type unknown [%d]\n",type);
	
	//Check if we got a different SSRC
	if (recv.SSRC!=ssrc)
	{
		//Check if it is a retransmission
		if (codec!=VideoCodec::RTX)
		{
			//Log
			Log("-RTPSession::ReadRTP() | New SSRC [new:%x,old:%x]\n",ssrc,recv.SSRC);
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
				Log("-RTPSession::ReadRTP() | Gor RTX for SSRC [rtx:%x,ssrc:%x]\n",recvRTX.SSRC,recv.SSRC);
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
				 //Exit
				 return Error("-RTPSession::ReadRTP() | RTP RTX packet apt type unknown [%d]\n",type);
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
			//Delete red packet
			delete(red);
			//Exit
			return Error("-RTPSession::ReadRTP() | RTP packet type unknown for primary type of redundant data [%d,rd:%d]\n",t,codec);
		}
		
		if (media==MediaFrame::Video && !isRTX) UltraDebug("RTX: Got  %d:%s primary %d:%s packet #%d ts:%u\n",type,VideoCodec::GetNameFor((VideoCodec::Type)codec),t,VideoCodec::GetNameFor((VideoCodec::Type)c),red->GetSeqNum(),red->GetTimestamp());
		
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
				return Error("-RTPSession::ReadRTP() | RTP packet type unknown for primary type of secundary data [%d,%d,red:%d]\n",i,t,codec);
			}
			//Set it
			red->SetRedundantCodec(i,c);
		}
		//Set packet
		packet = red;
	} else {
		//Create normal packet
		packet = new RTPTimedPacket(media,buffer,size);
		if (media==MediaFrame::Video && !isRTX) UltraDebug("RTX: Got  %d:%s packet #%d ts:%u\n",type,VideoCodec::GetNameFor((VideoCodec::Type)codec),packet->GetSeqNum(),packet->GetTimestamp());
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

	if (lost) UltraDebug("RTX: Missing %d [nack:%d,diff:%llu,rtt:%llu]\n",lost,isNACKEnabled,getDifTime(&lastFPU),rtt);
	
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
	return 1;
}

void RTPSession::Start()
{
	//We are running
	running = true;

	//Create thread
	createPriorityThread(&thread,run,this,0);
}

void RTPSession::Stop()
{
	//Check thred
	if (!isZeroThread(thread))
	{
		//Not running
		running = false;

		//Signal the pthread this will cause the poll call to exit
		pthread_kill(thread,SIGIO);
		//Wait thread to close
		pthread_join(thread,NULL);
		//Nulifi thread
		setZeroThread(&thread);
	}
}

/***********************
* run
*       Helper thread function
************************/
void * RTPSession::run(void *par)
{
        Log("-RTPSession::run() | thread [%d,0x%x]\n",getpid(),par);

	//Block signals to avoid exiting on SIGUSR1
	blocksignals();

        //Obtenemos el parametro
        RTPSession *sess = (RTPSession *)par;

        //Ejecutamos
        sess->Run();
	//Exit
	return NULL;
}

/***************************
 * Run
 * 	Server running thread
 ***************************/
int RTPSession::Run()
{
	Log(">RTPSession::Run() | [%p]\n",this);

	//Set values for polling
	ufds[0].fd = simSocket;
	ufds[0].events = POLLIN | POLLERR | POLLHUP;
	ufds[1].fd = simRtcpSocket;
	ufds[1].events = POLLIN | POLLERR | POLLHUP;

	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(simSocket,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	fcntl(simSocket,F_SETFL,fsflags);

	fsflags = fcntl(simRtcpSocket,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	fcntl(simRtcpSocket,F_SETFL,fsflags);

	//Catch all IO errors
	signal(SIGIO,EmptyCatch);

	//Run until ended
	while(running)
	{
		//Wait for events
		if(poll(ufds,2,-1)<0)
			//Check again
			continue;

		if (ufds[0].revents & POLLIN)
			//Read rtp data
			ReadRTP();
		if (ufds[1].revents & POLLIN)
			//Read rtcp data
			ReadRTCP();

		if ((ufds[0].revents & POLLHUP) || (ufds[0].revents & POLLERR) || (ufds[1].revents & POLLHUP) || (ufds[0].revents & POLLERR))
		{
			//Error
			Log("-RTPSession::Run() | Pool error event [%d]\n",ufds[0].revents);
			//Exit
			break;
		}
	}

	Log("<RTPSession::Run()\n");
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

void RTPSession::ProcessRTCPPacket(const RTCPCompoundPacket *rtcp)
{
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
		DWORD lostPacketsSinceLastSR = total - recv.totalPacketsSinceLastSR + recv.nackedPacketsSinceLastSR;
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
	UltraDebug("-RTPSession::SetRTT() | [%dms,nack:%d]\n",rtt,isNACKEnabled);
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
		
		//Check if we ar encripted
		if (encript)
		{
			//Check  session
			if (!sendSRTPSession)
				//Error
				return Error("-RTPSession::ReSendPacket() | no sendSRTPSession\n");
			//Encript
			srtp_err_status_t err = srtp_protect(sendSRTPSession,data,&len);
			//Check error
			if (err!=srtp_err_status_ok)
			{
				//Check if got listener
				if (listener)
					//Request a I frame
					listener->onFPURequested(this);
				//Nothing
				return Error("-RTPSession::ReSendPacket() | Error protecting RTP packet [%d] sending intra instead\n",err);
			}
		}

		//Check len
		if (len)
		{
			Debug("-RTPSession::ReSendPacket() | %d %d\n",seq,ext);
			//Send packet
			sendto(simSocket,data,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
		}
	} else {
		Debug("-RTPSession::ReSendPacket() | %d:%d %d not found first %d sending intra instead\n",send.cycles,seq,ext,rtxs.size() ?  rtxs.begin()->first : 0);
		//Check if got listener
		if (listener)
			//Request a I frame
			listener->onFPURequested(this);
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


void RTPSession::onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize)
{
	Log("-RTPSession::onDTLSSetup() | [media: %s]\n",MediaFrame::TypeToString(media));

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
