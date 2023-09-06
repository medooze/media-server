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
#include <openssl/opensslconf.h>
#include <openssl/ossl_typ.h>
#include "log.h"
#include "assertions.h"
#include "tools.h"
#include "rtp.h"
#include "stunmessage.h"
#include "RTPTransport.h"

BYTE rtpEmpty[] = {0x80,0x14,0x00,0x00,0x00,0x00,0x00,0x00};

//srtp library initializers
class SRTPLib
{
public:
	SRTPLib()	{ srtp_init();	}
};
SRTPLib srtp;

DWORD RTPTransport::minLocalPort = 0;
DWORD RTPTransport::maxLocalPort = 0;
int RTPTransport::minLocalPortRange = 50;

bool RTPTransport::SetPortRange(int minPort, int maxPort)
{
	// mitPort should be even
	if ( minPort % 2 )
		minPort++;

	//Check port range is possitive
	if (!minPort || maxPort<=minPort)
		//Error
		return Error("-RTPTransport::SetPortRange() | port range invalid [%d,%d]\n",minPort,maxPort);

	//check min range ports
	if (maxPort && maxPort-minPort<minLocalPortRange)
	{
		//Error
		Error("-RTPTransport::SetPortRange() | port range too short %d, should be at least %d\n",maxPort-minPort,minLocalPortRange);
		//Correct
		maxPort = minPort+minLocalPortRange;
	}

	//check min range
	if (minPort && minPort<1024)
	{
		//Error
		Error("-RTPTransport::SetPortRange() | min rtp port is inside privileged range, increasing it\n");
		//Correct it
		minPort = 1024;
	}

	//Check max port
	if (maxPort>65535)
	{
		//Error
		Error("-RTPTransport::SetPortRange() | max rtp port is too high, decreasing it\n");
		//Correc it
		maxPort = 65535;
	}

	//Set range
	minLocalPort = minPort;
	maxLocalPort = maxPort;

	//Log
	Log("-RTPTransport::SetPortRange() | configured RTP/RTCP ports range [%d,%d]\n", minLocalPort, maxLocalPort);

	//OK
	return true;
}

/*************************
* RTPTransport
* 	Constructro
**************************/
RTPTransport::RTPTransport(Listener *listener) :
	rtpLoop(this),
	rtcpLoop(this),
	endpoint(rtpLoop),
	dtls(*this,rtpLoop,endpoint.GetTransport())
{
	this->listener = listener;
	//Init values
	simSocket = FD_INVALID;
	simRtcpSocket = FD_INVALID;
	simPort = 0;
	simRtcpPort = 0;
	//Not muxing
	muxRTCP = false;
	//No crypto
	encript = false;
	decript = false;
	send = NULL;
	recv = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	
	//No remote peer info
	recIP = INADDR_ANY;
	recPort = 0;
	prio = 0;

	//Not dumping
	dumping = false;
	
	//Preparamos las direcciones de envio
	memset(&sendAddr,       0,sizeof(struct sockaddr_in));
	memset(&sendRtcpAddr,   0,sizeof(struct sockaddr_in));

	//Set family
	sendAddr.sin_family     = AF_INET;
	sendRtcpAddr.sin_family = AF_INET;
}

/*************************
* ~RTPTransport
* 	Destructor
**************************/
RTPTransport::~RTPTransport()
{
	//Reset
	Reset();
}

void RTPTransport::Reset()
{
	Log("-RTPTransport reset\n");

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
	
	recIP = INADDR_ANY;
	recPort = 0;
	
	//Not muxing
	muxRTCP = false;
	//No cripto
	encript = false;
	decript = false;
	send = NULL;
	recv = NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	//Preparamos las direcciones de envio
	memset(&sendAddr,       0,sizeof(struct sockaddr_in));
	memset(&sendRtcpAddr,   0,sizeof(struct sockaddr_in));
	//Set family
	sendAddr.sin_family     = AF_INET;
	sendRtcpAddr.sin_family = AF_INET;
}

int RTPTransport::SetLocalCryptoSDES(const char* suite,const BYTE* key,const DWORD len)
{
	srtp_err_status_t err;
	srtp_policy_t policy;

	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	//Get cypher
	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
// Ignore coverity error: "srtp_crypto_policy_set_rtp_default" in "srtp_crypto_policy_set_rtp_default(&policy.rtcp)" looks like a copy-paste error.
// coverity[copy_paste_error]
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_256_GCM")==0) {
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: AEAD_AES_256_GCM\n");
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_128_GCM")==0) {
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: AEAD_AES_128_GCM\n");
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
	} else {
		return Error("-RTPTransport::SetLocalCryptoSDES() | Unknown cipher suite: %s", suite);
	}

	//Check sizes
	if (len!=(DWORD)policy.rtp.cipher_key_len)
		//Error
		return Error("-RTPTransport::SetLocalCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

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
		return Error("-RTPTransport::SetLocalCryptoSDES() | Failed to create local SRTP session | err:%d\n", err);
	
	//if we already got a send session don't leak it
	if (send)
		//Dealoacate
		srtp_dealloc(send);

	//Set send SSRTP sesion
	send = session;

	//Evrything ok
	return 1;
}

int RTPTransport::SetLocalCryptoSDES(const char* suite, const char* key64)
{
	//Log
	Log("-RTPTransport::SetLocalCryptoSDES() | [key:%s,suite:%s]\n",key64,suite);

	//Get lenght
	WORD len64 = strlen(key64);
	//Allocate memory for the key
// Ignore coverity error: Allocating insufficient memory for the terminating null of the string.
// coverity[alloc_strlen]
	BYTE sendKey[len64];
	//Decode
	WORD len = av_base64_decode(sendKey,key64,len64);

	//Set it
	return SetLocalCryptoSDES(suite,sendKey,len);
}


int RTPTransport::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	Log("-RTPTransport::SetLocalSTUNCredentials() | [frag:%s,pwd:%s]\n",username,pwd);
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


int RTPTransport::SetRemoteSTUNCredentials(const char* username, const char* pwd)
{
	Log("-RTPTransport::SetRemoteSTUNCredentials() |  [frag:%s,pwd:%s]\n",username,pwd);
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

int RTPTransport::SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint)
{
	Log("-RTPTransport::SetRemoteCryptoDTLS | [setup:%s,hash:%s,fingerprint:%s]\n",setup,hash,fingerprint);

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
		return Error("-RTPTransport::SetRemoteCryptoDTLS | Unknown setup");

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
		return Error("-RTPTransport::SetRemoteCryptoDTLS | Unknown hash");

	//Init DTLS
	return dtls.Init();
}

int RTPTransport::SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len)
{
	srtp_err_status_t err;
	srtp_policy_t policy;

	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		Log("-RTPTransport::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		Log("-RTPTransport::SetRemoteCryptoSDES() | suite: AES_CM_128_HMAC_SHA1_32\n");
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
// Ignore coverity error: "srtp_crypto_policy_set_rtp_default" in "srtp_crypto_policy_set_rtp_default(&policy.rtcp)" looks like a copy-paste error.
// coverity[copy_paste_error]
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // NOTE: Must be 80 for RTCP!
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		Log("-RTPTransport::SetRemoteCryptoSDES() | suite: AES_CM_128_NULL_AUTH\n");
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		Log("-RTPTransport::SetRemoteCryptoSDES() | suite: NULL_CIPHER_HMAC_SHA1_80\n");
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_256_GCM")==0) {
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: AEAD_AES_256_GCM\n");
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
	} else if (strcmp(suite,"AEAD_AES_128_GCM")==0) {
		Log("-RTPTransport::SetLocalCryptoSDES() | suite: AEAD_AES_128_GCM\n");
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
	} else {
		return Error("-RTPTransport::SetRemoteCryptoSDES() | Unknown cipher suite %s", suite);
	}

	//Check sizes
	if (len!=(DWORD)policy.rtp.cipher_key_len)
		//Error
		return Error("-RTPTransport::SetRemoteCryptoSDES() | Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

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
		return Error("-RTPTransport::SetRemoteCryptoSDES() | Failed to create remote SRTP session | err:%d\n", err);
	
	//if we already got a recv session don't leak it
	if (recv)
		//Dealoacate
		srtp_dealloc(recv);
	//Set it
	recv = session;

	//Everything ok
	return 1;
}

int RTPTransport::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	//Log
	Log("-RTPTransport::SetRemoteCryptoSDES() | [key:%s,suite:%s]\n",key64,suite);

	//Decript
	decript = true;

	//Get length
	WORD len64 = strlen(key64);
	//Allocate memory for the key
// Ignore coverity error: Allocating insufficient memory for the terminating null of the string.
// coverity[alloc_strlen]
	BYTE recvKey[len64];
	//Decode
	WORD len = av_base64_decode(recvKey,key64,len64);

	//Set it
	return SetRemoteCryptoSDES(suite,recvKey,len);
}

int RTPTransport::SetLocalPort(int recvPort)
{
	//Override
	simPort = recvPort;
	
	return simPort;
}

int RTPTransport::GetLocalPort()
{
	// Return local
	return simPort;
}

/***********************************
* SetRemotePort
*	Inicia la sesion rtp de video remota
***********************************/
int RTPTransport::SetRemotePort(char *ip,int sendPort)
{
	//Get ip addr
	DWORD ipAddr = inet_addr(ip);

	//If we already have one IP binded
	if (recIP!=INADDR_ANY)
		//Exit
		return Log("-RTPTransport::SetRemotePort() | NAT already binded sucessfully to [%s:%d]\n",inet_ntoa(sendAddr.sin_addr),recPort);

	//Ok, let's et it
	Log("-RTPTransport::SetRemotePort() | [%s:%u]\n",ip,sendPort);

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

void RTPTransport::SendEmptyPacket()
{
	//Open rtp
	(void)sendto(simSocket,rtpEmpty,sizeof(rtpEmpty),0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
	//If not muxing
	if (!muxRTCP)
		//Send
		(void)sendto(simRtcpSocket,rtpEmpty,sizeof(rtpEmpty),0,(sockaddr *)&sendRtcpAddr,sizeof(struct sockaddr_in));
}

/********************************
* Init
*	Inicia el control rtcp
********************************/
int RTPTransport::Init()
{
	int retries = 0;

	Log(">RTPTransport::Init()\n");

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
			simPort = (RTPTransport::GetMinPort()+(RTPTransport::GetMaxPort()-RTPTransport::GetMinPort())*double(rand()/double(RAND_MAX)));
			//Make even
			simPort &= 0xFFFFFFFE;
		}
		//Try to bind to port
		recAddr.sin_port = htons(simPort);
		//Bind the rtcp socket
// Ignore coverity error: "this->simSocket" is passed to a parameter that cannot be negative.
// coverity[negative_returns]
		if(bind(simSocket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
		{
			//Use random
			simPort = 0;
			//Try again
			continue;
		}
		//If port was random
		if (!simPort)
		{
			socklen_t len = sizeof(struct sockaddr_in);
			//Get binded port
			if (getsockname(simSocket,(struct sockaddr *)&recAddr,&len)!=0)
				//Try again
				continue;
			//Get final port
			simPort = ntohs(recAddr.sin_port);
		}
		//Create new sockets
		simRtcpSocket = socket(PF_INET,SOCK_DGRAM,0);
		//Next port
		simRtcpPort = simPort+1;
		//Try to bind to port
		recAddr.sin_port = htons(simRtcpPort);
		//Bind the rtcp socket
// Ignore coverity error: "this->simRtcpSocket" is passed to a parameter that cannot be negative.
// coverity[negative_returns]
		if(bind(simRtcpSocket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
		{
			//Use random
			simPort = 0;
			//Try again
			continue;
		}

#ifdef SO_PRIORITY
		//Set COS
		int cos = 5;
		(void)setsockopt(simSocket,     SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
		(void)setsockopt(simRtcpSocket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
#endif
		//Set TOS
		int tos = 0x2E;
		(void)setsockopt(simSocket,     IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
		(void)setsockopt(simRtcpSocket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
		//Everything ok
		Log("-RTPTransport::Init() | Got ports [%d,%d]\n",simPort,simRtcpPort);
		//Start receiving
		Start();
		//Dump
		char filename[256];
		snprintf(filename,255,"/tmp/%d-%p.pcap",simPort,this);
		if (dumping) pcap.Open(filename);
		//Done
		Log("<RTPTransport::Init()\n");
		//Opened
		return 1;
	}

	//Error
	Error("-RTPTransport::Init() | too many failed attemps opening sockets\n");

	//Failed
	return 0;
}

/*********************************
* End
*	Termina la todo
*********************************/
int RTPTransport::End()
{
	Log(">RTPTransport::End()\n");

	//Stop just in case
	Stop();

	if (dumping) pcap.Close();
	
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

	Log("<RTPTransport::End()\n");

	return 1;
}

int RTPTransport::SendRTCPPacket(Packet&& packet)
{
	//Check if we have sendinf ip address
	if (sendRtcpAddr.sin_addr.s_addr == INADDR_ANY && !muxRTCP)
	{
		//Debug
		Debug("-RTPTransport::SendPacket() | Error sending rtcp packet, no remote IP yet\n");
		//Exit
		return 0;
	}
	
	//Write udp packet
	if (dumping) pcap.WriteUDP(getTimeMS(),0x7F000001,5004,ntohl(sendAddr.sin_addr.s_addr),ntohs(sendAddr.sin_port),packet.GetData(),packet.GetSize());

	//If encripted
	if (encript)
	{
		//Check  session
		if (!send)
			return Error("-RTPTransport::SendPacket() | no send\n");
		//Protect
		int len = packet.GetSize();
		srtp_err_status_t err = srtp_protect_rtcp(send,packet.GetData(),&len);
		packet.SetSize(len);
		//Check error
		if (err!=srtp_err_status_ok)
			//Nothing
			return Error("-RTPTransport::SendPacket() | Error protecting RTCP packet [%d]\n",err);
	}

	//If muxin
	if (muxRTCP)
		//Send using RTP port
		rtpLoop.Send(ntohl(sendAddr.sin_addr.s_addr),ntohs(sendAddr.sin_port),std::move(packet));
	else
		//Send using RCTP port
		rtcpLoop.Send(ntohl(sendRtcpAddr.sin_addr.s_addr),ntohs(sendRtcpAddr.sin_port),std::move(packet));
	
	
	
	return 1;
}

int RTPTransport::SendRTPPacket(Packet&& packet)
{
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
			Log("-RTPTransport::SendPacket() | NAT: Now sending to [%s:%d].\n", inet_ntoa(sendAddr.sin_addr), recPort);
			//Check if using ice
			if (iceRemoteUsername && iceRemotePwd && iceLocalUsername)
			{
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
				
				//Create new mesage
				Packet localPacket = rtpLoop.GetPacketPool().pick();

				//Serialize and autenticate
				size_t len = request->AuthenticatedFingerPrint(localPacket.GetData(),localPacket.GetCapacity(),iceLocalPwd);

				//Resize
				localPacket.SetSize(len);
				
				//Send response
				rtpLoop.Send(ntohl(sendAddr.sin_addr.s_addr),ntohs(sendAddr.sin_port),std::move(localPacket));

				//Clean response
				delete(request);
			}
		} else {
			//Exit
			Debug("-RTPTransport::SendPacket() | No remote address \n");
			//Exit
			return 0;
		}
	}
	
	//Write udp packet
	if (dumping) pcap.WriteUDP(getTimeMS(),0x7F000001,5004,ntohl(sendAddr.sin_addr.s_addr),ntohs(sendAddr.sin_port),packet.GetData(),packet.GetSize());

	//Check if we ar encripted
	if (encript)
	{
		//Check  session
		if (send)
		{
			//Encript
			int len = packet.GetSize();
			srtp_err_status_t srtp_err_status = srtp_protect(send,packet.GetData(),&len);
			packet.SetSize(len);
			//Check error
			if (srtp_err_status!=srtp_err_status_ok)
				//Error
				return Error("-RTPTransport::SendPacket() | Error protecting RTP packet [%d]\n",srtp_err_status);
		} else {
			//Log
			Debug("-RTPTransport::SendPacket() | no srtp sending session configured\n");
			//Don't send
			return 0;
		}
	}

	//Send response
	rtpLoop.Send(ntohl(sendAddr.sin_addr.s_addr),ntohs(sendAddr.sin_port),std::move(packet));
	 
	return 1;
}

int RTPTransport::ReadRTCP(const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port)
{
	//TOOD: remove and use ipAddr/port directly
	sockaddr_in from_addr;
	//Receive from everywhere
	from_addr.sin_addr.s_addr = htonl(ipAddr);
	from_addr.sin_port = htons(port);

	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(data,size))
	{
		//Parse message
		STUNMessage *stun = STUNMessage::Parse(data,size);

		//It was not a valid STUN message
		if (! stun)
			//Error
			return Error("-RTPTransport::ReadRTCP() | failed to parse STUN message\n");

		//Get type and method
		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();

		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			//Create response
			STUNMessage* resp = stun->CreateResponse();
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(&from_addr);
			//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
			
			//Create new mesage
			size_t len = 0;
			Packet packet = rtpLoop.GetPacketPool().pick();
		
			//Check if we have local passworkd
			if (iceLocalPwd)
				//Serialize and autenticate
				len = resp->AuthenticatedFingerPrint(packet.GetData(),packet.GetCapacity(),iceLocalPwd);
			else
				//Do nto authenticate
				len = resp->NonAuthenticatedFingerPrint(packet.GetData(),packet.GetCapacity());
			
			//resize
			packet.SetSize(len);

			//Send response
			rtpLoop.Send(ipAddr,port,std::move(packet));
			
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
	if (!RTCPCompoundPacket::IsRTCP(data,size))
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
		Log("-RTPTransport::ReadRTCP() | Got first RTCP, sending to %s:%d with rtcp-muxing:%d\n",inet_ntoa(sendRtcpAddr.sin_addr),ntohs(sendRtcpAddr.sin_port),muxRTCP);
	}

	int len = size;
	
	//Decript
	if (decript)
	{
		//Check session
		if (!recv)
			return Error("-RTPTransport::ReadRTCP() | No recvSRTPSession\n");
		//unprotect
		srtp_err_status_t err = srtp_unprotect_rtcp(recv,(BYTE*)data,&len);
		//Check error
		if (err!=srtp_err_status_ok)
			return Error("-RTPTransport::ReadRTCP() | Error unprotecting rtcp packet [%d]\n",err);
	}
	//RTCP mux disabled
	muxRTCP = false;
	
	//Write udp packet
	if (dumping) pcap.WriteUDP(getTimeMS(),ntohl(from_addr.sin_addr.s_addr),ntohs(from_addr.sin_port),0x7F000001,5004,data,len);
	
	//Parse it
	listener->onRTCPPacket(data,len);
	
	//OK
	return 1;
}

/*********************************
* GetTextPacket
*	Lee el siguiente paquete de video
*********************************/
int RTPTransport::ReadRTP(const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port)
{
	//TOOD: remove and use ipAddr/port directly
	sockaddr_in from_addr;
	//Receive from everywhere
	from_addr.sin_addr.s_addr = htonl(ipAddr);
	from_addr.sin_port = htons(port);

	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(data,size))
	{
		//Parse it
		STUNMessage *stun = STUNMessage::Parse(data,size);

		//It was not a valid STUN message
		if (!stun)
			//Error
			return Error("-RTPTransport::ReadRTP() | failed to parse STUN message\n");

		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();

		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			//Create response
			STUNMessage* resp = stun->CreateResponse();
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(ipAddr,port);
			//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
			
			//Create new mesage
			size_t len = 0;
			Packet packet = rtpLoop.GetPacketPool().pick();
		
			//Check if we have local passworkd
			if (iceLocalPwd)
				//Serialize and autenticate
				len = resp->AuthenticatedFingerPrint(packet.GetData(),packet.GetCapacity(),iceLocalPwd);
			else
				//Do nto authenticate
				len = resp->NonAuthenticatedFingerPrint(packet.GetData(),packet.GetCapacity());
			
			//resize
			packet.SetSize(len);

			//Send response
			rtpLoop.Send(ipAddr,port,std::move(packet));

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
			UltraDebug("-RTPTransport::ReadRTP() | ICE: received bind request from [%s:%d] with candidate [prio:%d,use:%d] current:%d\n", inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port),candPrio,stun->HasAttribute(STUNMessage::Attribute::UseCandidate),prio);
	

			//If use candidate to a differentIP  is set or we don't have another IP address
			if (recIP==INADDR_ANY || 
				(
					(recIP==from_addr.sin_addr.s_addr && sendAddr.sin_addr.s_addr!=from_addr.sin_addr.s_addr) && 
					(stun->HasAttribute(STUNMessage::Attribute::UseCandidate) && candPrio>=prio)
				)
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
				Log("-RTPTransport::ReadRTP() | ICE: Now sending to [%s:%hu:%d] prio:%d\n",inet_ntoa(sendAddr.sin_addr), ntohs(sendAddr.sin_port),recIP, prio);
				
				//If we start receiving from a drifferent ip address or it is the first one
				if (recIP!=from_addr.sin_addr.s_addr)
					//Request a I frame
					listener->onRemotePeer(inet_ntoa(sendAddr.sin_addr), ntohs(sendAddr.sin_port));

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
				size_t len = 0;
				Packet packet = rtpLoop.GetPacketPool().pick();

				//Check remote pwd
				if (iceRemotePwd)
					//Serialize and autenticate
					len = request->AuthenticatedFingerPrint(packet.GetData(),packet.GetCapacity(),iceRemotePwd);
				else
					//Do nto authenticate
					len = request->NonAuthenticatedFingerPrint(packet.GetData(),packet.GetCapacity());

				//resize
				packet.SetSize(len);
				
				//Send response
				rtpLoop.Send(ipAddr,port,std::move(packet));

				//Clean response
				delete(request);

				{
					// Needed for DTLS in client mode (otherwise the DTLS "Client Hello" is not sent over the wire)
					Packet packet;
					size_t len = dtls.Read(packet.GetData(),packet.GetCapacity());
					
					//Check it
					if (len>0)
					{
						//resize
						packet.SetSize(len);
						//Send response
						rtpLoop.Send(ipAddr,port,std::move(packet));
					}
				}
			}
		}

		//Delete message
		delete(stun);
		//Exit
		return 1;
	}

	//Check if it is RTCP
	if (RTCPCompoundPacket::IsRTCP(data,size))
	{
		int len = size;
		
		//Decript
		if (decript)
		{
			//Check session
			if (!recv)
				return Error("-RTPTransport::ReadRTP() | No recvSRTPSession\n");
			//unprotect
			srtp_err_status_t err = srtp_unprotect_rtcp(recv,(BYTE*)data,&len);
			//Check error
			if (err!=srtp_err_status_ok)
				return Error("-RTPTransport::ReadRTP() | Error unprotecting rtcp packet [%d]\n",err);
		}

		//RTCP mux enabled
		muxRTCP = true;
		
		//Write udp packet
		if (dumping) pcap.WriteUDP(getTimeMS(),ntohl(from_addr.sin_addr.s_addr),ntohs(from_addr.sin_port),0x7F000001,5004,data,len);
		
		//Handle incomming rtcp packets
		listener->onRTCPPacket(data,len);
		
		//Skip
		return 1;
	}

	//Check if it a DTLS packet
	if (DTLSConnection::IsDTLS(data,size))
	{
		//Feed it
		dtls.Write(data,size);

		//REad dtls data
		Packet packet = rtpLoop.GetPacketPool().pick();
		size_t len = dtls.Read(packet.GetData(),packet.GetCapacity());
					
		//Check it
		if (len>0)
		{
			//resize
			packet.SetSize(len);
			//Send response
			rtpLoop.Send(ipAddr,port,std::move(packet));
		}

		//Exit
		return 1;
	}

	//If we start receiving from a drifferent ip address or it is the first one
	if (recIP!=from_addr.sin_addr.s_addr)
	{
		//Log
		Log("-RTPTransport::ReadRTP() | NAT: received packet from new source [%s:%d]\n", inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));
		//Request a I frame for start sending
		listener->onRemotePeer(inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));
	}

	//Get receiving ip address
	recIP = from_addr.sin_addr.s_addr;
	//Get also port
	recPort = ntohs(from_addr.sin_port);
	
	//Check minimum size for rtp packet
	if (size<12)
	{
		//Debug
		Debug("-RTPTransport::ReadRTP() | RTP data not big enought[%ld]\n",size);
		//Exit
		return 0;
	}

	int len = size;
	//Check if it is encripted
	if (decript)
	{
		srtp_err_status_t err;
		//Check session
		if (!recv)
			return Error("-RTPTransport::ReadRTP() | No recvSRTPSession\n");
		//unprotect
		err = srtp_unprotect(recv,(BYTE*)data,&len);
		//Check status
		if (err!=srtp_err_status_ok)
			//Error
			return Error("-RTPTransport::ReadRTP() | Error unprotecting rtp packet [%d]\n",err);
	}
	
	//Write udp packet
	if (dumping) pcap.WriteUDP(getTimeMS(),ntohl(from_addr.sin_addr.s_addr),ntohs(from_addr.sin_port),0x7F000001,5004,data,size);
	
	listener->onRTPPacket(data,len);
	
	//Done
	return 1;
}

void RTPTransport::Start()
{
	//Start loops
	rtpLoop.Start(simSocket);
	rtcpLoop.Start(simRtcpSocket);
}

void RTPTransport::Stop()
{
	//Stop loops
	rtpLoop.Stop();
	rtcpLoop.Stop();
}

void RTPTransport::OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port)
{
	if (fd == simSocket)
		//Read rtp data
		ReadRTP(data,size,ipAddr,port);
	else if (fd == simRtcpSocket)
		//Read rtcp data
		ReadRTCP(data,size,ipAddr,port);
}

void RTPTransport::onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize)
{
	Log("-RTPTransport::onDTLSSetup()\n");

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
			Error("-TPTransport::onDTLSSetup() Unknown suite\n");
	}
}

void RTPTransport::onDTLSPendingData()
{
	//Until depleted
	while(true)
	{
		Packet packet = rtpLoop.GetPacketPool().pick();
		//Read from dtls
		size_t len = dtls.Read(packet.GetData(),packet.GetCapacity());
		if (!len)
			break;
		//resize
		packet.SetSize(len);
		//Send response
		rtpLoop.Send(ntohl(sendAddr.sin_addr.s_addr),ntohs(sendAddr.sin_port),std::move(packet));
	}
		
}
void RTPTransport::onDTLSSetupError()
{
	
}
void RTPTransport::onDTLSShutdown()
{
	
}
