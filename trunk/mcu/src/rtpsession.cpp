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
RTPSession::RTPSession(MediaFrame::Type media,Listener *listener) : remoteRateControl(this)
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
	sendSeq = 0;
	sendTime = random();
	sendLastTime = sendTime;
	sendSSRC = random();
	sendSR = 0;
	recExtSeq = 0;
	recSSRC = 0;
	recCycles = 0;
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
	//Empty stats
	numRecvPackets = 0;
	numSendPackets = 0;
	totalRecvBytes = 0;
	totalSendBytes = 0;
	lostRecvPackets = 0;
	totalRecvPacketsSinceLastSR = 0;
	totalRecvBytesSinceLastSR = 0;
	minRecvExtSeqNumSinceLastSR = RTPPacket::MaxExtSeqNum;
	jitter = 0;
	//No reports
	setZeroTime(&lastSR);
	setZeroTime(&lastReceivedSR);
	rtt = 0;
	//No cripto
	encript = false;
	decript = false;
	sendSRTPSession = NULL;
	recvSRTPSession = NULL;
	recvSRTPSessionRTX = NULL;
	sendKey		= NULL;
	recvKey		= NULL;
	//No ice
	iceLocalUsername = NULL;
	iceLocalPwd = NULL;
	iceRemoteUsername = NULL;
	iceRemotePwd = NULL;
	//NO FEC
	useFEC = false;
	useNACK = false;
	isNACKEnabled = false;
	//Fill with 0
	memset(sendPacket,0,MTU);
	//Preparamos las direcciones de envio
	memset(&sendAddr,       0,sizeof(struct sockaddr_in));
	memset(&sendRtcpAddr,   0,sizeof(struct sockaddr_in));
	//No thread
	thread = NULL;
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
	//If secure
	if (sendSRTPSession)
		//Dealoacate
		srtp_dealloc(sendSRTPSession);
	//If secure
	if (recvSRTPSession)
		//Dealoacate
		srtp_dealloc(recvSRTPSession);
	//If RTX
	if (recvSRTPSessionRTX)
		//Dealoacate
		srtp_dealloc(recvSRTPSessionRTX);
	if (sendKey)
		free(sendKey);
	if (recvKey)
		free(recvKey);
	if (cname)
		free(cname);
	//Delete rtx packets
	for (RTPOrderedPackets::iterator it = rtxs.begin(); it!=rtxs.end();++it)
	{
		//Get pacekt
		RTPTimedPacket *pkt = it->second;
		//Delete object
		delete(pkt);
	}
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
	//policy.allow_repeat_tx  = 1; <--- Not all srtp libs containts it
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
int RTPSession::SetProperties(const Properties& properties)
{
	//For each property
	for (Properties::const_iterator it=properties.begin();it!=properties.end();++it)
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
		} else if (it->first.compare("useFEC")==0) {
			//Set fec decoding
			useFEC = atoi(it->second.c_str());
		} else if (it->first.compare("useNACK")==0) {
			//Set fec decoding
			useNACK = atoi(it->second.c_str());
			//Enable NACK until first RTT
			isNACKEnabled = useNACK;
		} else {
			Error("Unknown RTP property [%s]\n",it->first.c_str());
		}
	}
	
	return 1;
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


	//Create new
	err = srtp_create(&recvSRTPSessionRTX,&policy);

	//Check error
	if (err!=err_status_ok)
		//Error
		Error("------------------------------------Failed set remote RTX SDES  (%d)\n", err);

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
		return Error("-SetSendingCodec error: no out RTP map\n");

	//Try to find it in the map
	for (RTPMap::iterator it = rtpMapOut->begin(); it!=rtpMapOut->end(); ++it)
	{
		//Is it ourr codec
		if (it->second==codec)
		{
			//Get type
			DWORD type = it->first;
			//Log it
			Log("-SetSendingCodec [codec:%s,type:%d]\n",GetNameForCodec(media,codec),type);
			//Set type in header
			((rtp_hdr_t *)sendPacket)->pt = type;
			//Set type
			sendType = type;
			//and we are done
			return true;
		}
	}

	//Not found
	return Error("-SetSendingCodec error: codec mapping not found [codec:%s]\n",GetNameForCodec(media,codec));
}

/***********************************
* SetRemotePort
*	Inicia la sesion rtp de video remota 
***********************************/
int RTPSession::SetRemotePort(char *ip,int sendPort)
{
	//Get ip addr
	DWORD ipAddr = inet_addr(ip);

	//If we already have one and it is a NATed
	if (recIP!=INADDR_ANY && ipAddr==INADDR_ANY)
		//Exit
		return Log("-SetRemotePort NAT already binded sucessfully to [%s:%d]\n",inet_ntoa(sendAddr.sin_addr),recPort);

	//Ok, let's et it
	Log("-SetRemotePort [%s:%d]\n",ip,sendPort);

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

void RTPSession::SetRemoteRateEstimator(RemoteRateEstimator* estimator)
{
	Log("-SetRemoteRateEstimator\n");

	//Store it
	remoteRateEstimator = estimator;
}

/********************************
* Init
*	Inicia el control rtcp
********************************/
int RTPSession::Init()
{
	int retries = 0;
	
	Log(">Init RTPSession\n");

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
			close(simSocket);
			//No socket
			simSocket = FD_INVALID;
		}
		//If we have a rtcp socket
		if (simRtcpSocket!=FD_INVALID)
		{
			///Close it
			close(simRtcpSocket);
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
			//Try again
			continue;
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

	//Not running;
	running = false;
	//If got socket
	if (simSocket!=FD_INVALID)
	{
		//Will cause poll to return
		close(simSocket);
		//No sockets
		simSocket = FD_INVALID;
	}
	if (simRtcpSocket!=FD_INVALID)
	{
		//Will cause poll to return
		close(simRtcpSocket);
		//No sockets
		simRtcpSocket = FD_INVALID;
	}

	return 1;
}

int RTPSession::SendPacket(RTCPCompoundPacket &rtcp)
{
	int ret = 0;
	BYTE data[MTU];
	DWORD size = MTU;

	//Check if we have sendinf ip address
	if (sendRtcpAddr.sin_addr.s_addr == INADDR_ANY && !muxRTCP)
		//Exit
		return Error("-No rtcp ip\n");

	//Serialize
	int len = rtcp.Serialize(data,size);
	//Check result
	if (!len)
		//Error
		return Error("Error serializing RTCP packet\n");

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
		//Send using RTP port
		ret = sendto(simSocket,data,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
	else
		//Send using RCTP port
		ret = sendto(simRtcpSocket,data,len,0,(sockaddr *)&sendRtcpAddr,sizeof(struct sockaddr_in));

	//Check error
	if (ret<0)
		//Return
		return Error("-Error sending RTP packet [%d]\n",errno);

	//Exit
	return 1;
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
			Log("-RTPSession NAT: Now sending %s to [%s:%d].\n", MediaFrame::TypeToString(media),inet_ntoa(sendAddr.sin_addr), recPort);
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
			return Error("-No remote address\n");
	}

	//Check if we need to send SR
	if (isZeroTime(&lastSR) || getDifTime(&lastSR)>1000000)
		//Send it
		SendSenderReport();
	
	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;

	//Init send packet
	headers->version = RTP_VERSION;
	headers->ssrc = htonl(sendSSRC);

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
	{
		//Encript
		err_status_t err = srtp_protect(sendSRTPSession,sendPacket,&len);
		//Check error
		if (err!=err_status_ok)
			//Nothing
			return Error("Error protecting RTCP packet [%d]\n",err);
	}

	//Send packet
	int ret = sendto(simSocket,sendPacket,len,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Inc stats
	numSendPackets++;
	totalSendBytes += packet.GetMediaLength();

	//Add it rtx queue
	if (useNACK)
	{
		//Create new pacekt
		RTPTimedPacket *rtx = new RTPTimedPacket(media,sendPacket,len);
		//Block
		rtxUse.WaitUnusedAndLock();
		//Add it to que
		rtxs[rtx->GetExtSeqNum()] = rtx;
		//Get time for packets to discard
		QWORD until = getTime()/1000 - fmax(rtt*2,100);
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
		rtxUse.Unlock();
	}

	//Exit
	return (ret>0);
}
int RTPSession::ReadRTCP()
{
	BYTE buffer[MTU];
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Read rtcp socket
	int size = recvfrom(simRtcpSocket,buffer,MTU,MSG_DONTWAIT,(sockaddr*)&from_addr, &from_len);

	//if got estimator
	if (remoteRateEstimator)
		//Update estimator
		remoteRateEstimator->Update(size);

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
			DWORD len = 0;
			//Create response
			STUNMessage* resp = stun->CreateResponse();
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(&from_addr);
			//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
			//Create  response
			DWORD size = resp->GetSize();
			BYTE *aux = (BYTE*)malloc(size);

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
		Log("-Got first RTCP, sending to %s:%d with rtcp-muxing:%d\n",inet_ntoa(sendRtcpAddr.sin_addr),ntohs(sendRtcpAddr.sin_port),muxRTCP);
	}
	
	//Decript
	if (decript)
	{
		//unprotect
		err_status_t err = srtp_unprotect_rtcp(recvSRTPSession,buffer,&size);
		//Check error
		if (err!=err_status_ok)
			return Error("Error unprotecting rtcp packet [%d]\n",err);
	}
	//RTCP mux disabled
	muxRTCP = false;
	//Parse it
	RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(buffer,size);
	//Check packet
	if (rtcp)
		//Handle incomming rtcp packets
		ProcessRTCPPacket(rtcp);
	
	//OK
	return 1;
}

/*********************************
* GetTextPacket
*	Lee el siguiente paquete de video
*********************************/
int RTPSession::ReadRTP()
{
	BYTE data[MTU];
	BYTE *buffer = data;
	sockaddr_in from_addr;
	bool isRTX = false;
	DWORD from_len = sizeof(from_addr);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Leemos del socket
	int size = recvfrom(simSocket,buffer,MTU,MSG_DONTWAIT,(sockaddr*)&from_addr, &from_len);

	//if got estimator
	if (remoteRateEstimator)
		//Update estimator
		remoteRateEstimator->Update(size);

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
			DWORD len = 0;
			//Create response
			STUNMessage* resp = stun->CreateResponse();
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(&from_addr);
			//TODO: Check incoming request username attribute value starts with iceLocalUsername+":"
			//Create  response
			DWORD size = resp->GetSize();
			BYTE *aux = (BYTE*)malloc(size);

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

			//If set
			//if (stun->HasAttribute(STUNMessage::Attribute::UseCandidate))
			//Check if ip's are different
			if (recIP!=from_addr.sin_addr.s_addr)
			{
				//Bind it to received packet ip
				recIP = from_addr.sin_addr.s_addr;
				//Get also port
				recPort = ntohs(from_addr.sin_port);
				//Log
				Log("-RTPSession NAT: received bind request from [%s:%d]\n", inet_ntoa(from_addr.sin_addr), recPort);
				//Check if got listener
				if (listener)
					//Request a I frame
					listener->onFPURequested(this);
				
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
		return 1;
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
				return Error("Error unprotecting rtcp packet [%d]\n",err);
		}
		//RTCP mux enabled
		muxRTCP = true;
		//Parse it
		RTCPCompoundPacket* rtcp = RTCPCompoundPacket::Parse(buffer,size);
		//Check packet
		if (rtcp)
			//Handle incomming rtcp packets
			ProcessRTCPPacket(rtcp);
		
		//Skip
		return 1;
	}

	//If we don't have originating IP
	if (recIP==INADDR_ANY)
	{
		//Bind it to first received packet ip
		recIP = from_addr.sin_addr.s_addr;
		//Get also port
		recPort = ntohs(from_addr.sin_port);
		//Log
		Log("-RTPSession NAT: received packet from [%s:%d]\n", inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));
		//Check if got listener
		if (listener)
			//Request a I frame
			listener->onFPURequested(this);
	}

	//Check minimum size for rtp packet
	if (size<12)
		return 0;

	//This should be improbed
	if (useNACK && recSSRC && recSSRC!=RTPPacket::GetSSRC(buffer))
	{
		Debug("-----nacked %x %x\n",recSSRC,RTPPacket::GetSSRC(buffer));
		//It is a retransmited packet
		isRTX = true;
	}
	
	//Check if it is encripted
	if (decript)
	{
		err_status_t err;
		//Check if it is a retransmited packet
		if (!isRTX)
			//unprotect
			err = srtp_unprotect(recvSRTPSession,buffer,&size);
		else
			//unprotect RTX
			err = srtp_unprotect(recvSRTPSessionRTX,buffer,&size);
		//Check status
		if (err!=err_status_ok)
			//Error
			return Error("Error unprotecting rtp packet [%d]\n",err);
	}

	//If it is a retransmission
	if (isRTX)
	{
		//Get original sequence number
		WORD osn = get2(buffer,sizeof(rtp_hdr_t));
		//Move origin
		for (int i=sizeof(rtp_hdr_t)-1;i>=0;--i)
			//Move
			buffer[i+2] = buffer[i];
		//Move init
		buffer+=2;
		//Set original seq num
		set2(buffer,2,osn);
	}

	//Get type
	BYTE type = RTPPacket::GetType(buffer);

	//Check rtp map
	if (!rtpMapIn)
		//Error
		return Error("-RTP map not set\n");
	
	//Set initial codec
	BYTE codec = rtpMapIn->GetCodecForType(type);

	//Check codec
	if (codec==RTPMap::NotFound)
		//Exit
		return Error("-RTP packet type unknown [%d]\n",type);

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
			return Error("-RTP packet type unknown for primary type of redundant data [%d]\n",t);
		}
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
				return Error("-RTP packet type unknown for primary type of secundary data [%d,%d]\n",i,t);
			}
			//Set it
			red->SetRedundantCodec(i,c);
		}
		//Set packet
		packet = red;
	} else {
		//Create normal packet
		packet = new RTPTimedPacket(media,buffer,size);
	}

	//Set codec
	packet->SetCodec(codec);

	//Get ssrc
	DWORD ssrc = packet->GetSSRC();

	//If new ssrc
	if (recSSRC!=ssrc)
	{
		//Check if it is from the retransmission stream
		if (!isRTX)
		{
			Log("-New SSSRC [new:%x,old:%x]\n",ssrc,recSSRC);
			//Send SR to old one
			SendSenderReport();
			//Reset packets
			packets.Reset();
			//Reset cycles
			recCycles = 0;
			//Reset
			recExtSeq = 0;
			//Update ssrc
			recSSRC = ssrc;
			//If remote estimator
			if (remoteRateEstimator)
				//Add stream
				remoteRateEstimator->AddStream(recSSRC,&remoteRateControl);
		} else {
			//Set SSRC of the original stream
			packet->SetSSRC(recSSRC);
		}
	}
	
	//Get sec number
	WORD seq = packet->GetSeqNum();
	
	//Check if we have a sequence wrap
	if (seq<0x00FF && (recExtSeq & 0xFFFF)>0xFF00)
		//Increase cycles
		recCycles++;

	//Set cycles
	packet->SetSeqCycles(recCycles);

	//Update rate control
	remoteRateControl.Update(packet);

	//Increase stats
	numRecvPackets++;
	totalRecvPacketsSinceLastSR++;
	totalRecvBytes += size;
	totalRecvBytesSinceLastSR += size;

	//Get ext seq
	DWORD extSeq = packet->GetExtSeqNum();

	//Check if it is the min for this SR
	if (extSeq<minRecvExtSeqNumSinceLastSR)
		//Store minimum
		minRecvExtSeqNumSinceLastSR = extSeq;

	//If we have a not out of order pacekt
	if (extSeq>recExtSeq)
	{
		//Check possible lost pacekts
		if (recExtSeq && extSeq>(recExtSeq+1))
		{
			//Get number of lost packets
			WORD lost = extSeq-recExtSeq-1;
			//Base packet missing
			WORD base = recExtSeq+1;

			//Update estimator
			remoteRateControl.UpdateLost(lost);

			//If nack is enable t waiting for a PLI/FIR response (to not oeverflow)
			if (isNACKEnabled && !requestFPU)
			{
				Log("-Nacking %d lost %d\n",base,lost);

				//Generate new RTCP NACK report
				RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();

				//Send them
				while (lost>0)
				{
					//Skip base
					lost--;
					//Get number of lost in the 16 mask
					WORD n = lost;
					//Check nex 16 packets
					if (n>16)
						//Clip
						n = 16;
					//Create mask
					WORD mask = 0xFFFF << (16-n);
					//Create NACK
					RTCPRTPFeedback *nack = RTCPRTPFeedback::Create(RTCPRTPFeedback::NACK,sendSSRC,recSSRC);
					//Limit incoming bitrate
					nack->AddField( new RTCPRTPFeedback::NACKField(base,mask));
					//Add to packet
					rtcp->AddRTCPacket(nack);
					//Reduce lost
					lost -= n;
					//Increase base
					base += 16;
				}

				//Send packet
				SendPacket(*rtcp);

				//Delete it
				delete(rtcp);
			}
		}
		
		//Update seq num
		recExtSeq = extSeq;

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
			int v = d-jitter;
			//Calculate jitter
			jitter += v/16;
		}

		//Update rtp timestamp
		recTimestamp = packet->GetClockTimestamp();
	}

	//Check if we are using fec
	if (useFEC)
	{
		//Append to the FEC decoder
		if (fec.AddPacket(packet))
			//Append only packets with media data
			packets.Add(packet);
		//Try to recover
		RTPTimedPacket* recovered = fec.Recover();
		//If we have recovered a pacekt
		while(recovered)
		{
			//Log
			Log("packet receovered!!!!\n");
			//Overwrite time with the time of the original 
			recovered->SetTime(packet->GetTime());
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
			//add recovered packet
			packets.Add(recovered);
			//Try to recover another one (yuhu!)
			recovered = fec.Recover();
		}
	} else {
		//Add pacekt
		packets.Add(packet);
	}

	//Check if we need to send SR
	if (isZeroTime(&lastSR) || getDifTime(&lastSR)>1000000)
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
	return packets.Wait();
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
				//Get Timestamp, the middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) received as part of the most recent RTCP sender report (SR) packet from source SSRC_n. If no SR has been received yet, the field is set to zero.
				recSR = sr->GetTimestamp() >> 16;
				//Uptade last received SR
				getUpdDifTime(&lastReceivedSR);
				//Check recievd report
				for (int j=0;j<sr->GetCount();j++)
				{
					//Get report
					RTCPReport *report = sr->GetReport(j);
					//Check ssrc
					if (report->GetSSRC()==sendSSRC)
					{
						//Calculate RTT
						if (!isZeroTime(&lastSR) && report->GetLastSR() == sendSR)
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
				RTCPReceiverReport* rr = (RTCPReceiverReport*)packet;
				//Check recievd report
				for (int j=0;j<rr->GetCount();j++)
				{
					//Get report
					RTCPReport *report = rr->GetReport(j);
					//Check ssrc
					if (report->GetSSRC()==sendSSRC)
					{
						//Calculate RTT
						if (!isZeroTime(&lastSR) && report->GetLastSR() == sendSR)
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
				RTCPRTPFeedback *fb = (RTCPRTPFeedback*) packet;
				//Check feedback type
				switch(fb->GetFeedbackType())
				{
					case RTCPRTPFeedback::NACK:
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							RTCPRTPFeedback::NACKField *field = (RTCPRTPFeedback::NACKField*) fb->GetField(i);
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
						Log("-TempMaxMediaStreamBitrateRequest\n");
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							RTCPRTPFeedback::TempMaxMediaStreamBitrateField *field = (RTCPRTPFeedback::TempMaxMediaStreamBitrateField*) fb->GetField(i);
							//Check if it is for us
							if (listener && field->GetSSRC()==sendSSRC)
								//call listener
								listener->onTempMaxMediaStreamBitrateRequest(this,field->GetBitrate(),field->GetOverhead());
						}
						break;
					case RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification:
						Log("-TempMaxMediaStreamBitrateNotification\n");
						pendingTMBR = false;
						if (requestFPU)
						{
							requestFPU = false;
							SendFIR();
						}
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get field
							RTCPRTPFeedback::TempMaxMediaStreamBitrateField *field = (RTCPRTPFeedback::TempMaxMediaStreamBitrateField*) fb->GetField(i);

						}
		
						break;
				}
				break;
			}
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
						Log("-SliceLossIndication\n");
						break;
					case RTCPPayloadFeedback::ReferencePictureSelectionIndication:
						Log("-ReferencePictureSelectionIndication\n");
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffRequest:
						Log("-TemporalSpatialTradeOffRequest\n");
						break;
					case RTCPPayloadFeedback::TemporalSpatialTradeOffNotification:
						Log("-TemporalSpatialTradeOffNotification\n");
						break;
					case RTCPPayloadFeedback::VideoBackChannelMessage:
						Log("-VideoBackChannelMessage\n");
						break;
					case RTCPPayloadFeedback::ApplicationLayerFeeedbackMessage:
						for (BYTE i=0;i<fb->GetFieldCount();i++)
						{
							//Get feedback
							RTCPPayloadFeedback::ApplicationLayerFeeedbackField* msg = (RTCPPayloadFeedback::ApplicationLayerFeeedbackField*)fb->GetField(i);
							//Get size and payload
							DWORD len	= msg->GetLength();
							BYTE* payload	= msg->GetPayload();
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
				break;
		}
	}
	//Delete pacekt
	delete(rtcp);
}

RTCPCompoundPacket* RTPSession::CreateSenderReport()
{
	timeval tv;

	//Create packet
	RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();

	//Get now
	gettimeofday(&tv, NULL);

	//Create Sender report
	RTCPSenderReport *sr = new RTCPSenderReport();

	//Append data
	sr->SetSSRC(sendSSRC);
	sr->SetTimestamp(&tv);
	sr->SetRtpTimestamp(sendLastTime);
	sr->SetOctectsSent(totalSendBytes);
	sr->SetPacketsSent(numSendPackets);

	//Update time of latest sr
	DWORD sinceLastSR = getUpdDifTime(&lastSR);

	//If we have received somthing
	if (totalRecvPacketsSinceLastSR && recExtSeq>=minRecvExtSeqNumSinceLastSR)
	{
		//Get number of total packtes
		DWORD total = recExtSeq - minRecvExtSeqNumSinceLastSR + 1;
		//Calculate lost
		DWORD lostRecvPacketsSinceLastSR = total - totalRecvPacketsSinceLastSR;
		//Add to total lost count
		lostRecvPackets += lostRecvPacketsSinceLastSR;
		//Calculate fraction lost
		DWORD frac = (lostRecvPacketsSinceLastSR*256)/total;

		//Create report
		RTCPReport *report = new RTCPReport();

		//Set SSRC of incoming rtp stream
		report->SetSSRC(recSSRC);

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
		report->SetLastJitter(jitter);
		report->SetLostCount(lostRecvPackets);
		report->SetLastSeqNum(recExtSeq);

		//Reset data
		totalRecvPacketsSinceLastSR = 0;
		totalRecvBytesSinceLastSR = 0;
		minRecvExtSeqNumSinceLastSR = RTPPacket::MaxExtSeqNum;

		//Append it
		sr->AddReport(report);
	}

	//Append SR to rtcp
	rtcp->AddRTCPacket(sr);

	//Store last send SR 32 middle bits
	sendSR = sr->GetNTPSec() << 16 | sr->GetNTPFrac() >> 16;

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
	rtcp->AddRTCPacket(sdes);

	//Return it
	return rtcp;
}

int RTPSession::SendSenderReport()
{
	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	//If we have not got a notification from latest TMBR
	if (pendingTMBR)
	{
		//Resend TMMBR
		RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest,sendSSRC,recSSRC);
		//Limit incoming bitrate
		rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(recSSRC,pendingTMBBitrate,0));
		//Add to packet
		rtcp->AddRTCPacket(rfb);
	} else if (remoteRateEstimator)	{
		//Get lastest estimation and convert to kbps
		DWORD estimation = remoteRateEstimator->GetEstimatedBitrate();
		//If it was ok
		if (estimation)
		{
			std::list<DWORD> ssrcs;
			//Get ssrcs
			remoteRateEstimator->GetSSRCs(ssrcs);
			//Create feedback
			// SSRC of media source (32 bits):  Always 0; this is the same convention as in [RFC5104] section 4.2.2.2 (TMMBN).
			RTCPPayloadFeedback *remb = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::ApplicationLayerFeeedbackMessage,sendSSRC,0);
			//Send estimation
			remb->AddField(RTCPPayloadFeedback::ApplicationLayerFeeedbackField::CreateReceiverEstimatedMaxBitrate(ssrcs,estimation));
			//Add to packet
			rtcp->AddRTCPacket(remb);
			//Resend TMMBR
			RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest,sendSSRC,recSSRC);
			//Limit incoming bitrate
			rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(recSSRC,estimation,0));
			//Add to packet
			rtcp->AddRTCPacket(rfb);
		}
	}
	
	//Send packet
	int ret = SendPacket(*rtcp);

	//Delete it
	delete(rtcp);

	//Exit
	return ret;
}

int RTPSession::SendFIR()
{
	Log("-SendFIR\n");

	//Send all the packets inmediatelly to the decoderso I frame can be handled as soon as possoble
	packets.HurryUp();

	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	//Create fir request
	RTCPPayloadFeedback *fir = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::FullIntraRequest,sendSSRC,recSSRC);
	//ADD field
	fir->AddField(new RTCPPayloadFeedback::FullIntraRequestField(recSSRC,firReqNum++));
	//Add to rtcp
	rtcp->AddRTCPacket(fir);

	//Add PLI
	RTCPPayloadFeedback *pli = RTCPPayloadFeedback::Create(RTCPPayloadFeedback::PictureLossIndication,sendSSRC,recSSRC);
	//Add to rtcp
	rtcp->AddRTCPacket(pli);

	//Send packet
	int ret = SendPacket(*rtcp);

	//Delete it
	delete(rtcp);
	
	//Exit
	return ret;
}

int RTPSession::RequestFPU()
{
	//packets.Reset();
	if (!pendingTMBR)
	{
		//request FIR
		SendFIR();
	} else {
		//Wait for TMBN response to no overflow
		requestFPU = true;
	}
}

void RTPSession::SetRTT(DWORD rtt)
{
	//Set it
	this->rtt = rtt;
	//Uptade the rate control
	remoteRateControl.UpdateRTT(rtt);
	//if got estimator
	if (remoteRateEstimator)
		//Update estimator
		remoteRateEstimator->SetRTT(rtt);
	//If it is video
	if (media==MediaFrame::Video)
		//Update
		packets.SetMaxWaitTime(fmax(rtt,120)*2);
	//Check RTT to enable NACK
	if (useNACK)
		//Enable NACK only if RTT is small
		isNACKEnabled = (rtt < 180);
}

void RTPSession::onTargetBitrateRequested(DWORD bitrate)
{
	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	//Create TMMBR
	RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateRequest,sendSSRC,recSSRC);
	//Limit incoming bitrate
	rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(recSSRC,bitrate,0));
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

void RTPSession::ReSendPacket(int seq)
{
	//Lock
	rtxUse.IncUse();

	//Calculate ext seq number
	DWORD ext = ((DWORD)recCycles)<<16 | seq;

	//Find packet to retransmit
	RTPOrderedPackets::iterator it = rtxs.find(ext);

	//If we still have it
	if (it!=rtxs.end())
	{
		//Get packet
		RTPTimedPacket* packet = it->second;
		//Re send pacekt
		sendto(simSocket,packet->GetData(),packet->GetSize(),0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
	}

	//Unlock
	rtxUse.DecUse();
}
int RTPSession::SendTempMaxMediaStreamBitrateNotification(DWORD bitrate,DWORD overhead)
{
	Log("-SendTempMaxMediaStreamBitrateNotification [%d,%d]\n",bitrate,overhead);

	//Create rtcp sender retpor
	RTCPCompoundPacket* rtcp = CreateSenderReport();

	//Create TMMBR
	RTCPRTPFeedback *rfb = RTCPRTPFeedback::Create(RTCPRTPFeedback::TempMaxMediaStreamBitrateNotification,sendSSRC,recSSRC);
	//Limit incoming bitrate
	rfb->AddField( new RTCPRTPFeedback::TempMaxMediaStreamBitrateField(sendSSRC,bitrate,0));
	//Add to packet
	rtcp->AddRTCPacket(rfb);

	//Send packet
	int ret = SendPacket(*rtcp);

	//Delete it
	delete(rtcp);

	//Exit
	return ret;
}

