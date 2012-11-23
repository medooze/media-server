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
#include <srtp/srtp.h>
#include <time.h>
#include "log.h"
#include "tools.h"
#include "rtp.h"
#include "rtpsession.h"
#include "stunmessage.h"
#include <libavutil/base64.h>

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

bool RTPSession::SetPortRange(int minPort, int maxPort)
{
	// mitPort should be even
	if ( minPort % 2 )
		minPort++;

	//Check port range is possitive
	if (maxPort<minPort)
		//Error
		return Error("-RTPSession port range invalid [%d,%d]\n",minPort,maxPort);

	//check min range ports
	if (maxPort-minPort<50)
	{
		//Error
		Error("-RTPSession port raange too short %d, should be at least 50\n",maxPort-minPort);
		//Correct
		maxPort = minPort+50;
	}

	//check min range
	if (minPort<1024)
	{
		//Error
		Error("-RTPSession min rtp port is inside privileged range, increasing it\n");
		//Correct it
		minPort = 1024;
	}

	//Check max port
	if (maxPort>65535)
	{
		//Error
		Error("-RTPSession max rtp port is too high, decreasing it\n");
		//Correc it
		maxPort = 65535;
	}
	
	//Set range
	minLocalPort = minPort;
	maxLocalPort = maxPort;

	//Log
	Log("-RTPSession configured RTP/RTCP ports range [%d,%d]\n", minLocalPort, maxLocalPort);

	//OK
	return true;
}

/*************************
* RTPSession
* 	Constructro
**************************/
RTPSession::RTPSession(MediaFrame::Type media,Listener *listener)
{
	//Store listener
	this->listener = listener;
	//And media
	this->media = media;
	//Init values
	sendType = -1;
	simSocket = 0;
	simRtcpSocket = 0;
	simPort = 0;
	simRtcpPort = 0;
	sendSeq = 0;
	sendTime = random();
	sendLastTime = sendTime;
	sendSSRC = random();
	recSeq = (WORD)-1;
	recSSRC = 0;
	recCycles = 0;
	recTimestamp = 0;
	recSR = 0;
	setZeroTime(&recTimeval);
	recIP = INADDR_ANY;
	recPort = 0;
	//Not muxing
	muxRTCP = false;
	//Default cname
	cname = strdup("default@localhost");
	//Empty types by defauilt
	rtpMapIn = NULL;
	rtpMapOut = NULL;
	//Empty stats
	numRecvPackets = 0;
	numSendPackets = 0;
	totalRecvBytes = 0;
	totalSendBytes = 0;
	lostRecvPackets = 0;
	lostRecvPacketsSinceLastSR = 0;
	totalRecvPacketsSinceLastSR = 0;
	lost = 0;
	//No reports
	setZeroTime(&lastSR);
	//No cripto
	encript = false;
	decript = false;
	sendSRTPSession = NULL;
	recvSRTPSession = NULL;
	sendKey		= NULL;
	recvKey		= NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	//Fill with 0
	memset(sendPacket,0,MTU);
	//No thread
	thread = NULL;
	running = false;
	//Set version
	((rtp_hdr_t *)sendPacket)->version = RTP_VERSION;
}

/*************************
* ~RTPSession
* 	Destructor
**************************/
RTPSession::~RTPSession()
{
	if (rtpMapIn)
		delete(rtpMapIn);
	if (rtpMapOut)
		delete(rtpMapOut);
	//For each codec
	while(packets.Length())
		//Delete
		delete(packets.Pop());
	//Clean mem
	if (iceLocalUsername)
		free(iceLocalUsername);
	if (iceLocalPwd)
		free(iceLocalPwd);
	//If secure
	if (sendSRTPSession)
		//Dealoacate
		srtp_dealloc(sendSRTPSession);
	//If secure
	if (recvSRTPSession)
		//Dealoacate
		srtp_dealloc(recvSRTPSession);
	if (sendKey)
		free(sendKey);
	if (recvKey)
		free(recvKey);
	if (cname)
		free(cname);
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
	err_status_t err;
	srtp_policy_t policy;

	Log("-Set local RTP SDES [suite:%s,key:%s]\n",suite,key64);

	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	//Get cypher
	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else {
		return Error("Unknown cipher suite");
	}
	//Get lenght
	WORD len64 = strlen(key64);
	//Allocate memory for the key
	sendKey = (BYTE*)malloc(len64);
	//Decode
	WORD len = av_base64_decode(sendKey,key64,len64);

	//Check sizes
	if (len!=policy.rtp.cipher_key_len)
		//Error
		return Error("Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

	//Set polciy values
	policy.ssrc.type	= ssrc_any_outbound;
	policy.ssrc.value	= 0;
	policy.key		= sendKey;
	policy.next		= NULL;

	//Create new
	err = srtp_create(&sendSRTPSession,&policy);
	
	//Check error
	if (err!=err_status_ok)
		//Error
		return Error("Failed to create srtp session (%d)\n", err);

	//Decript
	encript = true;

	//Evrything ok
	return 1;
}
int RTPSession::SetProperties(const RTPSession::Properties& properties)
{
	//For each property
	for (RTPSession::Properties::const_iterator it=properties.begin();it!=properties.end();++it)
	{
		Log("Setting RTP property [%s:%s]\n",it->first.c_str(),it->second.c_str());
		
		//Check
		if (it->first.compare("rtcp-mux")==0)
		{
			//Set rtcp muxing
			muxRTCP = atoi(it->second.c_str());
		} else if (it->first.compare("ssrc")==0) {
			//Set ssrc for sending
			sendSSRC = atoi(it->second.c_str());
		} else if (it->first.compare("cname")==0) {
			//Check if already got one
			if (cname)
				//Delete it
				free(cname);
			//Clone
			cname = strdup(it->second.c_str());
		} else {
			Error("Unknown RTP property [%s]\n",it->first.c_str());
		}
	}
	
	return 0;
}
int RTPSession::SetLocalSTUNCredentials(const char* username, const char* pwd)
{
	Log("-SetLocalSTUNCredentials [frag:%s,pwd:%s]\n",username,pwd);
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
	Log("-SetRemoteSTUNCredentials [frag:%s,pwd:%s]\n",username,pwd);
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
int RTPSession::SetRemoteCryptoSDES(const char* suite, const char* key64)
{
	err_status_t err;
	srtp_policy_t policy;

	Log("-Set remote RTP SDES [suite:%s,key:%s]\n",suite,key64);
	
	//empty policy
	memset(&policy, 0, sizeof(srtp_policy_t));

	if (strcmp(suite,"AES_CM_128_HMAC_SHA1_80")==0)
	{
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_HMAC_SHA1_32")==0) {
		crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	} else if (strcmp(suite,"AES_CM_128_NULL_AUTH")==0) {
		crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
		crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
	} else if (strcmp(suite,"NULL_CIPHER_HMAC_SHA1_80")==0) {
		crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
		crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
	} else {
		return Error("Unknown cipher suite");
	}
	//Get lenght
	WORD len64 = strlen(key64);
	//Allocate memory for the key
	BYTE* recvKey = (BYTE*)malloc(len64);
	//Decode
	WORD len = av_base64_decode(recvKey,key64,len64);

	//Check sizes
	if (len!=policy.rtp.cipher_key_len)
		//Error
		return Error("Key size (%d) doesn't match the selected srtp profile (required %d)\n",len,policy.rtp.cipher_key_len);

	//Set polciy values
	policy.ssrc.type	= ssrc_any_inbound;
	policy.ssrc.value	= 0;
	policy.key		= recvKey;
	policy.next		= NULL;

	//Create new
	err = srtp_create(&recvSRTPSession,&policy);

	//Check error
	if (err!=err_status_ok)
		//Error
		return Error("Failed set remote SDES  (%d)\n", err);

	//Decript
	decript = true;
	
	//Everything ok
	return 1;
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

/***********************************
* SetLocalPort
*	Inicia la sesion rtp de video local
***********************************/
int RTPSession::GetLocalPort()
{
	// Return local
	return simPort;
}

/***********************************
* SetRemotePort
*	Inicia la sesion rtp de video remota
***********************************/
void RTPSession::SetSendingType(int type)
{
	Log("-SetSendingType [type:%d]\n",type);

	//Set type in header
	((rtp_hdr_t *)sendPacket)->pt = type;

	//Set type
	sendType = type;
}

bool RTPSession::SetSendingCodec(DWORD codec)
{
	//If we have rtp map
	if (rtpMapOut)
	{
		//Try to find it in the map
		for (RTPMap::iterator it = rtpMapOut->begin(); it!=rtpMapOut->end(); ++it)
		{
			//Is it ourr codec
			if (it->second==codec)
			{
				//Set it
				SetSendingType(it->first);
				//and we are done
				return true;
			}
		}
	} else {
		//Send codec as it is
		SetSendingType(codec);
		//It is ok for now
		return true;
	}

	//Not found
	return Error("RTP codec mapping not found\n");
}

/***********************************
* SetRemotePort
*	Inicia la sesion rtp de video remota 
***********************************/
int RTPSession::SetRemotePort(char *ip,int sendPort)
{
	Log("-SetRemotePort [%s:%d]\n",ip,sendPort);

	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;
	
	//Set it in packet
	headers->ssrc = htonl(sendSSRC);

	//Preparamos las direcciones de envio
	memset(&sendAddr,       0,sizeof(struct sockaddr_in));
	memset(&sendRtcpAddr,   0,sizeof(struct sockaddr_in));
	
	//Los rellenamos
	sendAddr.sin_family     	= AF_INET;
	sendRtcpAddr.sin_family 	= AF_INET;

	//Ip y puerto de destino
	sendAddr.sin_addr.s_addr 	= inet_addr(ip);
	sendRtcpAddr.sin_addr.s_addr 	= inet_addr(ip);
	sendAddr.sin_port 		= htons(sendPort);
	sendRtcpAddr.sin_port 		= htons(sendPort+1);

	//Open rtp and rtcp ports
	sendto(simSocket,rtpEmpty,sizeof(rtpEmpty),0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Y abrimos los sockets	
	return 1;
}

int RTPSession::SendEmptyPacket()
{
	//Check if we have sendinf ip address
	if (sendAddr.sin_addr.s_addr == INADDR_ANY)
		//Exit
		return 0;
	
	//Open rtp and rtcp ports
	sendto(simSocket,rtpEmpty,sizeof(rtpEmpty),0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//ok
	return 1;
}

/********************************
* Init
*	Inicia el control rtcp
********************************/
int RTPSession::Init()
{
	int retries = 0;
	
	Log(">Init RTPSession\n");

	sockaddr_in sendAddr;

	//Clear addr
	memset(&sendAddr,0,sizeof(struct sockaddr_in));
	//Set family
	sendAddr.sin_family = AF_INET;

	//Get two consecutive ramdom ports
	while (retries++<100)
	{
		//If we have a rtp socket
		if (simSocket)
		{
			// Close first socket
			close(simSocket);
			//No socket
			simSocket = 0;
		}
		//If we have a rtcp socket
		if (simRtcpSocket)
		{
			///Close it
			close(simRtcpSocket);
			//No socket
			simRtcpSocket = 0;
		}

		//Create new sockets
		simSocket = socket(PF_INET,SOCK_DGRAM,0);
		//Get random
		simPort = (RTPSession::GetMinPort()+(RTPSession::GetMaxPort()-RTPSession::GetMinPort())*double(rand()/double(RAND_MAX)));
		//Make even
		simPort &= 0xFFFFFFFE;
		//Try to bind to port
		sendAddr.sin_port = htons(simPort);
		//Bind the rtcp socket
		if(bind(simSocket,(struct sockaddr *)&sendAddr,sizeof(struct sockaddr_in))!=0)
			//Try again
			continue;
		//Create new sockets
		simRtcpSocket = socket(PF_INET,SOCK_DGRAM,0);
		//Next port
		simRtcpPort = simPort+1;
		//Try to bind to port
		sendAddr.sin_port = htons(simRtcpPort);
		//Bind the rtcp socket
		if(bind(simRtcpSocket,(struct sockaddr *)&sendAddr,sizeof(struct sockaddr_in))!=0)
			//Try again
			continue;
		//Everything ok
		Log("-Got ports [%d,%d]\n",simPort,simRtcpPort);
		//Start receiving
		Start();
		//Done
		Log("<Init RTPSession\n");
		//Opened
		return 1;
	}
	//Error
	Error("RTPSession too many failed attemps opening sockets");
	
	//Failed
	return 0;
}

/*********************************
* End
*	Termina la todo
*********************************/
int RTPSession::End()
{
	//Stop just in case
	Stop();

	//If got socket
	if (simSocket || simRtcpSocket)
	{
		//Not running;
		running = false;
		//Will cause poll to return
		close(simSocket);
		close(simRtcpSocket);
		//No sockets
		simSocket = 0;
		simRtcpSocket = 0;
	}

	return 1;
}

int RTPSession::SendPacket(RTCPCompoundPacket &rtcp)
{
	int ret = 0;
	BYTE data[MTU];
	DWORD size = MTU;

	//Serialize
	int len = rtcp.Serialize(data,size);
	//Check result
	if (!len)
		//Error
		return 0;

	//If encripted
	if (encript)
	{
		//Protect
		err_status_t err = srtp_protect_rtcp(sendSRTPSession,data,&len);
		//Check error
		if (err!=err_status_ok)
			//Nothing
			return Error("Error protecting RTCP packet [%d]\n",err);
	}

	//If muxin
	if (muxRTCP)
		//Send using RTP 5 tuple
		ret = sendto(simSocket,data,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
	else
		//Send using RCTP 5 tuple
		ret = sendto(simRtcpSocket,data,len,0,(sockaddr *)&sendRtcpAddr,sizeof(struct sockaddr_in));

	//Exit
	return (ret>0);
}

int RTPSession::SendPacket(RTPPacket &packet)
{
	return SendPacket(packet,packet.GetTimestamp());
}

int RTPSession::SendPacket(RTPPacket &packet,DWORD timestamp)
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
			Log("-RTPSession NAT: Now sending to [%s:%d].\n", inet_ntoa(sendAddr.sin_addr), recPort);
			//Check if using ice
			if (iceRemoteUsername && iceRemotePwd && iceLocalUsername)
			{
				BYTE aux[MTU];
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
				request->AddAttribute(STUNMessage::Attribute::IceControlled,(QWORD)-1);
				request->AddAttribute(STUNMessage::Attribute::UseCandidate);
				request->AddAttribute(STUNMessage::Attribute::Priority,(DWORD)33554431);

				//Create  request
				DWORD size = request->GetSize();

				//Serialize and autenticate
				int len = request->AuthenticatedFingerPrint(aux,size,iceRemotePwd);
				//Send it
				sendto(simSocket,aux,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

				//Clean response
				delete(request);
			}
		} else
			//Exit
			return 0;
	}

	//Check if we need to send SR
	if (isZeroTime(&lastSR) || getDifTime(&lastSR)>1000000)
		//Send it
		SendSenderReport();
	
	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;

	//Calculate last timestamp
	sendLastTime = sendTime + timestamp;

	//POnemos el timestamp
	headers->ts=htonl(sendLastTime);

	//Incrementamos el numero de secuencia
	headers->seq=htons(sendSeq++);

	//La marca de fin de frame
	headers->m=packet.GetMark();

	//Calculamos el inicio
	int ini = sizeof(rtp_hdr_t);

	//Comprobamos que quepan
	if (ini+packet.GetMediaLength()>MTU)
		return Error("SendPacket Overflow [size:%d,max:%d]\n",ini+packet.GetMediaLength(),MTU);

	//Copiamos los datos
        memcpy(sendPacket+ini,packet.GetMediaData(),packet.GetMediaLength());
	
	//Set pateckt length
	int len = packet.GetMediaLength()+ini;

	//Check if we ar encripted
	if (encript)
		//Encrip
		srtp_protect(sendSRTPSession,sendPacket,&len);

	//Mandamos el mensaje
	int ret = sendto(simSocket,sendPacket,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Inc stats
	numSendPackets++;
	totalSendBytes += packet.GetMediaLength();

	//Exit
	return (ret>0);
}
void RTPSession::ReadRTCP()
{
	BYTE buffer[MTU];
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Read rtcp socket
	int size = recvfrom(simRtcpSocket,buffer,MTU,MSG_DONTWAIT,(sockaddr*)&from_addr, &from_len);

	//Check if it is an STUN request
	STUNMessage *stun = STUNMessage::Parse(buffer,size);
	
	//If it was
	if (stun)
	{
		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();
		
		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			//If set
			if (stun->HasAttribute(STUNMessage::Attribute::UseCandidate))
			{
				//Create response
				STUNMessage* resp = stun->CreateResponse();
				//Add received xor mapped addres
				resp->AddXorAddressAttribute(&from_addr);
				//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
				//Create  response
				DWORD size = resp->GetSize();
				BYTE *aux = (BYTE*)malloc(size);

				//Serialize and autenticate
				DWORD len = resp->AuthenticatedFingerPrint(aux,size,iceLocalPwd);
				//Send it
				sendto(simRtcpSocket,aux,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

				//Clean memory
				free(aux);
				//Clean response
				delete(resp);
			}
		}

		//Delete message
		delete(stun);
		//Exit
		return;
	}

	//Check if it is RTCP
	if (RTCPCompoundPacket::IsRTCP(buffer,size))
	{
		//Decript
		if (decript)
		{
			//unprotect
			err_status_t err = srtp_unprotect_rtcp(recvSRTPSession,buffer,&size);
			//Check error
			if (err!=err_status_ok)
			{
				Error("Error decoding packet [%d]\n",err);
				//Nothing
				return;
			}
		}
		//RTCP mux disabled
		muxRTCP = false;
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(buffer,size);
		//Handle incomming rtcp packets
		ProcessRTCPPacket(rtcp);
	}
}

/*********************************
* GetTextPacket
*	Lee el siguiente paquete de video
*********************************/
void RTPSession::ReadRTP()
{
	BYTE buffer[MTU];
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Leemos del socket
	int size = recvfrom(simSocket,buffer,MTU,MSG_DONTWAIT,(sockaddr*)&from_addr, &from_len);

	//Check if it is an STUN request
	STUNMessage *stun = STUNMessage::Parse(buffer,size);
	
	//If it was
	if (stun)
	{
		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();
		
		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			//If set
			if (stun->HasAttribute(STUNMessage::Attribute::UseCandidate))
			{
				//Create response
				STUNMessage* resp = stun->CreateResponse();
				//Add received xor mapped addres
				resp->AddXorAddressAttribute(&from_addr);
				//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
				//Create  response
				DWORD size = resp->GetSize();
				BYTE *aux = (BYTE*)malloc(size);

				//Serialize and autenticate
				DWORD len = resp->AuthenticatedFingerPrint(aux,size,iceLocalPwd);
				//Send it
				sendto(simSocket,aux,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

				//Clean memory
				free(aux);
				//Clean response
				delete(resp);

				//If we don't have originating IP
				if (recIP==INADDR_ANY)
				{
					//Bind it to first received packet ip
					recIP = from_addr.sin_addr.s_addr;
					//Get also port
					recPort = ntohs(from_addr.sin_port);
					//Log
					Log("-RTPSession NAT: received bind request from [%s:%d]\n", inet_ntoa(from_addr.sin_addr), recPort);
					//Check if got listener
					if (listener)
						//Request a I frame
						listener->onFPURequested(this);
				}
				if (iceRemoteUsername && iceRemotePwd)
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
					request->AddAttribute(STUNMessage::Attribute::IceControlled,(QWORD)-1);
					request->AddAttribute(STUNMessage::Attribute::UseCandidate);
					request->AddAttribute(STUNMessage::Attribute::Priority,(DWORD)33554431);

					//Create  request
					DWORD size = request->GetSize();
					BYTE* aux = (BYTE*)malloc(size);

					//Serialize and autenticate
					DWORD len = request->AuthenticatedFingerPrint(aux,size,iceRemotePwd);
					//Send it
					sendto(simSocket,aux,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

					//Clean memory
					free(aux);
					//Clean response
					delete(request);
				}
			}
		}

		//Delete message
		delete(stun);
		//Exit
		return;
	}

	//Check if it is RTCP
	if (RTCPCompoundPacket::IsRTCP(buffer,size))
	{
		//Decript
		if (decript)
		{
			//unprotect
			err_status_t err = srtp_unprotect_rtcp(recvSRTPSession,buffer,&size);
			//Check error
			if (err!=err_status_ok)
			{
				Error("Error decoding packet [%d]\n",err);
				//Nothing
				return;
			}
		}
		//RTCP mux enabled
		muxRTCP = true;
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(buffer,size);
		//Handle incomming rtcp packets
		ProcessRTCPPacket(rtcp);
		//Skip
		return;
	}

	//If we don't have originating IP
	if (recIP==INADDR_ANY)
	{
		//Bind it to first received packet ip
		recIP = from_addr.sin_addr.s_addr;
		//Get also port
		recPort = ntohs(from_addr.sin_port);
		//Log
		Log("-RTPSession NAT: received packet from [%s:%d]\n", inet_ntoa(from_addr.sin_addr), recPort);
		//Check if got listener
		if (listener)
			//Request a I frame
			listener->onFPURequested(this);
	}

	//Check minimum size for rtp packet
	if (size<12)
		return;
	
	//Check if it is encripted
	if (decript)
	{
		//unprotect
		err_status_t err = srtp_unprotect(recvSRTPSession,buffer,&size);
		//Check error
		if (err!=err_status_ok)
		{
			Error("Error decoding packet [%d]\n",err);
			//Nothing
			return;
		}
	}

	//Create rtp packet
	RTPPacket *packet = new RTPPacket(media,buffer,size);

	//Obtenemos el tipo
	BYTE type = packet->GetType();

	//Check maps
	if (rtpMapIn)
	{
		//Find the type in the map
		RTPMap::iterator it = rtpMapIn->find(type);
		//Check it is in there
		if (it==rtpMapIn->end())
		{
			//Delete pacekt
			delete(packet);
			//Exit
			return;
		}
		//It is our codec
		packet->SetCodec(it->second);
	} else
		//Set codec
		packet->SetCodec(type);

	//Increase stats
	numRecvPackets++;
	totalRecvPacketsSinceLastSR++;
	totalRecvBytes += size;

	//Get sec number
	WORD seq = packet->GetSeqNum();

	//Get ssrc
	DWORD ssrc = packet->GetSSRC();

	//Check SSRC
	if (recSSRC==(DWORD)-1 || recSSRC==ssrc)
	{
		//If it is no first
		if (recSeq!=(WORD)-1)
		{
			//Check sequence number
			if (seq<=recSeq)
			{
				//Check if it is an out of order or duplicate packet
				if (recSeq-seq<255)
				{
					//Ignore it
					return;
				} else if (seq==1 && recSeq==(WORD)-1) {
					//Not lost, just seq wrapping
					lost = 0;
					//Increase cycles
					recCycles++;
				} else {
					//Too many
					lost = (WORD)-1;
					//Reset stats
					recCycles = 0;
				}
			} else if (seq-(recSeq+1)<255) {
				//Get the number of lost packets
				lost = seq-(recSeq+1);
			} else {
				//Too much
				lost = (WORD)-1;
			}
		} else {
			//Not lost
			lost = 0;
		}
	} else if (recSSRC!=ssrc) {
		//Change of SSRC
		//Reset cycles
		recCycles = 0;
		//Lost
		lost = (WORD)-1;
	} else {
		//Lost
		lost = (WORD)-1;
	}

	//Check if it has not been something extrange
	if (lost!=(WORD)-1)
	{
		//Increase lost packet count
		lostRecvPackets += lost;
		//And the lost packet for the SR
		lostRecvPacketsSinceLastSR += lost;
		//Also count it for the total
		totalRecvPacketsSinceLastSR += lost;
	}

	//Update ssrc
	recSSRC = ssrc;

	//Update last one
	recSeq = seq;

	//Get diff from previous
	DWORD diff = getUpdDifTime(&recTimeval)/1000;

	//if not fist packet
	if (recTimestamp)
	{
		//TODO: FIX rate should be inside packet
		DWORD rate = 1;
		if (media==MediaFrame::Audio)
			rate = 8;
		else if (media==MediaFrame::Video)
			rate = 90;
		//Get difference of arravail times
		DWORD d = (packet->GetTimestamp()-recTimestamp)/rate-diff;
		//Calculate jitter
		jitter += (abs(d)-jitter)/16;
	}
	
	//Update rtp timestamp
	recTimestamp = packet->GetTimestamp();

	//Push it back
	packets.Add(packet);

	//Check if we need to send SR
	if (isZeroTime(&lastSR) || getDifTime(&lastSR)>1000000)
		//Send it
		SendSenderReport();
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
	if (thread)
	{
		//Not running
		running = false;
		
		//Signal the pthread this will cause the poll call to exit
		pthread_kill(thread,SIGIO);
		//Wait thread to close
		pthread_join(thread,NULL);
		//Nulifi thread
		thread = NULL;
	}
}

/***********************
* run
*       Helper thread function
************************/
void * RTPSession::run(void *par)
{
        Log("-RTPSession thread [%d,0x%x]\n",getpid(),par);

	//Block signals to avoid exiting on SIGUSR1
	blocksignals();

        //Obtenemos el parametro
        RTPSession *sess = (RTPSession *)par;

        //Ejecutamos
        pthread_exit((void *)sess->Run());
}

/***************************
 * Run
 * 	Server running thread
 ***************************/
int RTPSession::Run()
{
	Log(">Run RTPSession [%p]\n",this);

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
			Log("Pool error event [%d]\n",ufds[0].revents);
			//Exit
			break;
		}
	}

	Log("<RTPSession run\n");
}

RTPPacket* RTPSession::GetPacket()
{
	//Wait for pacekts
	if (!packets.Wait(0))
		//We have been canceled
		return NULL;
	//Send it
	return packets.Pop();
}

void RTPSession::CancelGetPacket()
{
	//cancel
	packets.Cancel();
}

void RTPSession::ProcessRTCPPacket(RTCPCompoundPacket *rtcp)
{
	//For each packet
	for (int i = 0; i<rtcp->GetPacketCount();i++)
	{
		//Get pacekt
		RTCPPacket* packet = rtcp->GetPacket(i);
		//Check packet type
		switch (packet->GetType())
		{
			case RTCPPacket::SenderReport:
			{
				RTCPSenderReport* sr = (RTCPSenderReport*)packet;
				//Get Timestamp
				recSR = sr->GetTimestamp();
				break;
			}
			case RTCPPacket::ReceiverReport:
				break;
			case RTCPPacket::SDES:
				break;
			case RTCPPacket::Bye:
				break;
			case RTCPPacket::App:
				break;
			case RTCPPacket::RTPFeedback:
				break;
			case RTCPPacket::PayloadFeedback:
			{
				//Get feedback packet
				RTCPPayloadFeedback *fb = (RTCPPayloadFeedback*) packet;
				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPPayloadFeedback::PictureLossIndication:
					case RTCPPayloadFeedback::FullIntraRequest:
						//Chec listener
						if (listener)
							//Send intra refresh
							listener->onFPURequested(this);
						break;
					case RTCPPayloadFeedback::SliceLossIndication:
						break;
					case RTCPPayloadFeedback::ReferencePictureSelectionIndication:
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffRequest:
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffNotification:
						break;
					case RTCPPayloadFeedback::VideoBackChannelMessage:
						break;
					case RTCPPayloadFeedback:: ApplicationLayerFeeedbackMessage:
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
				break;
		}
	}
	//Delete pacekt
	delete(rtcp);
}

int RTPSession::SendSenderReport()
{
	RTCPCompoundPacket rtcp;
	timeval tv;
	
	//Get now
	gettimeofday(&tv, NULL);

	//Convert secs to NTP_JAN_1970;
	DWORD secs = tv.tv_sec + 2208988800UL;
	//Convert microsecods to 32 bits fraction
	DWORD frac = ((double)tv.tv_usec)*0.4294967296;

	//Create Sender report
	RTCPSenderReport *sr = new RTCPSenderReport();

	//Append data
	sr->SetSSRC(sendSSRC);
	sr->SetNTPSec(secs);
	sr->SetNTPFrac(frac);
	sr->SetRtpTimestamp(sendLastTime);
	sr->SetOctectsSent(totalSendBytes);
	sr->SetPacketsSent(numSendPackets);

	//If we have received somthing
	if (totalRecvPacketsSinceLastSR)
	{
		//Create report
		RTCPReport *report = new RTCPReport();

		//Set SSRC of incoming rtp stream
		report->SetSSRC(recSSRC);

		//Check last report time
		if (!isZeroTime(&lastSR))
			//Get time and update it
			report->SetDelaySinceLastSR(getDifTime(&lastSR)/1000);
		else
			//No previous SR
			report->SetDelaySinceLastSR(0);
		
		//The middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) received as part of the most recent RTCP sender report (SR) packet from source SSRC_n. If no SR has been received yet, the field is set to zero.
		report->SetLastSR(recSR>>16);
		//Calculate fraction lost
		DWORD frac = (lostRecvPacketsSinceLastSR*256)/totalRecvPacketsSinceLastSR;
		//Set it
		report->SetFractionLost(frac);
		//Reset
		totalRecvPacketsSinceLastSR = 0;
		lostRecvPacketsSinceLastSR = 0;

		//Set other data
		report->SetLastJitter(jitter);
		report->SetLostCount(lostRecvPackets);
		report->SetLastSeqNum(recCycles<<16 | recSeq);

		//Append it
		sr->AddReport(report);
	}

	//Append SR to rtcp
	rtcp.AddRTCPacket(sr);

	//Update time of latest sr
	getUpdDifTime(&lastSR);

	//Create SDES
	RTCPSDES* sdes = new RTCPSDES();
	
	//Create description
	RTCPSDES::Description *desc = new RTCPSDES::Description();
	//Set ssrc
	desc->SetSSRC(sendSSRC);
	//Add item
	desc->AddItem( new RTCPSDES::Item(RTCPSDES::Item::CName,cname));

	//Add it
	sdes->AddDescription(desc);

	//Add to rtcp
	rtcp.AddRTCPacket(sdes);

	//Send Sender Report
	return SendPacket(rtcp);
}
