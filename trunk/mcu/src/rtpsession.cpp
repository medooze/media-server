#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "tools.h"
#include "rtp.h"
#include "rtpsession.h"

#define SIMETRICSIGNALING 1

BYTE rtpEmpty[] = {0x80,0x14,0x00,0x00,0x00,0x00,0x00,0x00};

/*************************
* RTPSession
* 	Constructro
**************************/
RTPSession::RTPSession(Listener *listener)
{
	//Store listener
	this->listener = listener;
	//Init values
	sendType = -1;
	simSocket = 0;
	simRtcpSocket = 0;
	simPort = 0;
	simRtcpPort = 0;
	sendSeq = 0;
	sendTime = random();
	sendSSRC = random();
	recSeq = (WORD)-1;
	recSSRC = 0;
	recIP = INADDR_ANY;
	recPort = 0;
	//Empty types by defauilt
	rtpVideoMapIn = NULL;
	rtpVideoMapOut = NULL;
	rtpAudioMapIn = NULL;
	rtpAudioMapOut = NULL;
	rtpTextMapIn = NULL;
	rtpTextMapOut = NULL;
	//Empty stats
	numRecvPackets = 0;
	numSendPackets = 0;
	totalRecvBytes = 0;
	totalSendBytes = 0;
	lostRecvPackets = 0;

	//Fill with 0
	memset(sendPacket,0,sizeof(rtp_hdr_t));
	//Set version
	((rtp_hdr_t *)sendPacket)->version = RTP_VERSION;
}

/*************************
* ~RTPSession
* 	Destructor
**************************/
RTPSession::~RTPSession()
{
	if (rtpVideoMapIn)
		delete(rtpVideoMapIn);
	if (rtpVideoMapOut)
		delete(rtpVideoMapOut);
	if (rtpAudioMapIn)
		delete(rtpAudioMapIn);
	if (rtpAudioMapOut)
		delete(rtpAudioMapOut);
	if (rtpTextMapIn)
		delete(rtpTextMapIn);
	if (rtpTextMapOut)
		delete(rtpTextMapOut);
}


void RTPSession::SetSendingVideoRTPMap(VideoCodec::RTPMap &map)
{
	//If we already have one
	if (rtpVideoMapOut)
		//Delete it
		delete(rtpVideoMapOut);
	//Clone it
	rtpVideoMapOut = new VideoCodec::RTPMap(map);
}

void RTPSession::SetSendingAudioRTPMap(AudioCodec::RTPMap &map)
{
	//If we already have one
	if (rtpAudioMapOut)
		//Delete it
		delete(rtpAudioMapOut);
	//Clone it
	rtpAudioMapOut = new AudioCodec::RTPMap(map);
}

void RTPSession::SetSendingTextRTPMap(TextCodec::RTPMap &map)
{
	//If we already have one
	if (rtpTextMapOut)
		//Delete it
		delete(rtpTextMapOut);
	//Clone it
	rtpTextMapOut = new TextCodec::RTPMap(map);
}

void RTPSession::SetReceivingVideoRTPMap(VideoCodec::RTPMap &map)
{
	//If we already have one
	if (rtpVideoMapIn)
		//Delete it
		delete(rtpVideoMapIn);
	//Clone it
	rtpVideoMapIn = new VideoCodec::RTPMap(map);
}

void RTPSession::SetReceivingAudioRTPMap(AudioCodec::RTPMap &map)
{
	//If we already have one
	if (rtpAudioMapIn)
		//Delete it
		delete(rtpAudioMapIn);
	//Clone it
	rtpAudioMapIn = new AudioCodec::RTPMap(map);
}

void RTPSession::SetReceivingTextRTPMap(TextCodec::RTPMap &map)
{
	//If we already have one
	if (rtpTextMapIn)
		//Delete it
		delete(rtpTextMapIn);
	//Clone it
	rtpTextMapIn = new TextCodec::RTPMap(map);
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

bool RTPSession::SetSendingVideoCodec(VideoCodec::Type codec)
{
	//If we have rtp map
	if (rtpVideoMapOut)
	{
		//Try to find it in the map
		for (VideoCodec::RTPMap::iterator it = rtpVideoMapOut->begin(); it!=rtpVideoMapOut->end(); ++it)
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

bool RTPSession::SetSendingAudioCodec(AudioCodec::Type codec)
{
	//If we have rtp map
	if (rtpAudioMapOut)
	{
		//Try to find it in the map
		for (AudioCodec::RTPMap::iterator it = rtpAudioMapOut->begin(); it!=rtpAudioMapOut->end(); ++it)
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

bool RTPSession::SetSendingTextCodec(TextCodec::Type codec)
{
	//If we have rtp map
	if (rtpTextMapOut)
	{
		//Try to find it in the map
		for (TextCodec::RTPMap::iterator it = rtpTextMapOut->begin(); it!=rtpTextMapOut->end(); ++it)
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

	//Reset SSRC
	sendSSRC = random();

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
	Log(">Init RTPSession\n");

	sockaddr_in sendAddr;
	socklen_t len;

	// Set addres
	memset(&sendAddr,0,sizeof(struct sockaddr_in));
	sendAddr.sin_family = AF_INET;
	// Create sockets
	simSocket = socket(PF_INET,SOCK_DGRAM,0);

	//Bind
	bind(simSocket,(struct sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
	// Get rtp socket port
	len = sizeof(struct sockaddr_in);
	getsockname(simSocket,(struct sockaddr *)&sendAddr,&len);
	simPort = ntohs(sendAddr.sin_port);

	// Open rtcp socket
	simRtcpSocket = socket(PF_INET,SOCK_DGRAM,0);
	// Try get next port
	sendAddr.sin_port = htons(simPort+1);
	// Bind
	bind(simRtcpSocket,(struct sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Get rtcp port
	len = sizeof(struct sockaddr_in);
	getsockname(simRtcpSocket,(struct sockaddr *)&sendAddr,&len);
	simRtcpPort = ntohs(sendAddr.sin_port);

	// Ensure they are consecutive
	while ( simPort%2 || simPort+1!=simRtcpPort )
	{
		// Close first socket
		close(simSocket);
		// Reuse previus opened socket
		simSocket = simRtcpSocket;
		simPort = simRtcpPort;
		// Create new socket
		simRtcpSocket = socket(PF_INET,SOCK_DGRAM,0);
		// if we have a port
		if (simPort)
			// Get next port
			sendAddr.sin_port = htons(simPort+1);
		else
			// Get random port
			sendAddr.sin_port = htons(0);
		//Bind the rtcp socket
		bind(simRtcpSocket,(struct sockaddr *)&sendAddr,sizeof(struct sockaddr_in));
		//And get rtcp port
		len = sizeof(struct sockaddr_in);
		getsockname(simRtcpSocket,(struct sockaddr *)&sendAddr,&len);
		simRtcpPort = ntohs(sendAddr.sin_port);
		Log("-Got ports [%d,%d]\n",simPort,simRtcpPort);
	}

	Log("<Init RTPSession\n");

	//Opened
	return 1;
}


/*********************************
* End
*	Termina la todo
*********************************/
int RTPSession::End()
{
	//Cerramos los sockets
	close(simSocket);
	close(simRtcpSocket);
	return 1;
}

/*********************************
* SendVideoPacket
*	Manda un packete de video
*********************************/
int RTPSession::SendVideoPacket(BYTE* data,int size,int last,DWORD frameTime)
{
	//Check if we have sendinf ip address
	if (sendAddr.sin_addr.s_addr == INADDR_ANY)
	{
		//Do we have rec ip?
		if (recIP!=INADDR_ANY)
		{
			//Do NAT
			sendAddr.sin_addr.s_addr = recIP;
			//Get port
			sendAddr.sin_port = htons(recPort);
			//Log
			Log("-RTPSession NAT: Now sending to [%s:%d].\n", inet_ntoa(sendAddr.sin_addr), recPort);
		} else
			//Exit
			return 0;
	}
	
	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;

	//Calculamos el incremento en multiplos de 30 fps a 90Khz 
	int ts = sendTime+frameTime;
	
	ts=(ts/2880)*2880;

	//POnemos el timestamp
	headers->ts=htonl(ts);

	//Incrementamos el numero de secuencia
	headers->seq=htons(sendSeq++);

	//La marca de fin de frame
	headers->m=last;

	//Si es el ultimo actualizamos el videoTime
	if (last)
		sendTime+=frameTime;

	//Calculamos el inicio
	int ini= sizeof(rtp_hdr_t) -4;

	//Si tiene cc
	if (headers->cc)
		ini+=4;

	//Comprobamos que quepan
	if (ini+size>MTU)
	{
		//Salimos
		Log("SendVideoPacket Overflow\n");
		return 0;
	}

	//Copiamos los datos
        memcpy(sendPacket+ini,data,size);

	//Mandamos el mensaje
	int ret = sendto(simSocket,sendPacket,size+ini,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Inc stats
	numSendPackets++;
	totalSendBytes += size;

	//Exit
	return (ret>0); 
}

/*********************************
* SendPacket
*	Manda un packete de video
*********************************/
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
		} else
			//Exit
			return 0;
	}
	
	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;

	//POnemos el timestamp
	headers->ts=htonl(timestamp);

	//Incrementamos el numero de secuencia
	headers->seq=htons(sendSeq++);

	//La marca de fin de frame
	headers->m=packet.GetMark();

	//Calculamos el inicio
	int ini = sizeof(rtp_hdr_t) -4;

	//Si tiene cc
	if (headers->cc)
		ini+=4;

	//Comprobamos que quepan
	if (ini+packet.GetMaxMediaLength()>MTU)
	{
		//Salimos
		Log("SendVideoPacket Overflow\n");
		return 0;
	}

	//Copiamos los datos
        memcpy(sendPacket+ini,packet.GetMediaData(),packet.GetMediaLength());

	//Mandamos el mensaje
	int ret = sendto(simSocket,sendPacket,packet.GetMediaLength()+ini,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Inc stats
	numSendPackets++;
	totalSendBytes += packet.GetMediaLength();

	//Exit
	return (ret>0);
}

/**********************************
* WaitForSocket
*	Espera a que haya datos que leer de un socket
**********************************/
int RTPSession::WaitForSocket(int fd, int secs)
{
	fd_set rfds;
	struct timeval tv;

	//Rellemanos el set
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	//Vamos a esperar medio segundo
	tv.tv_sec = secs;
	tv.tv_usec = 0;

	//Esperamos a que haya datos
	int ret =  select(fd+1,&rfds, NULL, NULL, &tv);

	return ret>0;
}

/*********************************
* GetVideoPacket
*	Lee el siguiente paquete de video
*********************************/
int RTPSession::GetVideoPacket(BYTE *data,DWORD *size,BYTE *last,BYTE *lost,VideoCodec::Type *codec,DWORD *timestamp)
{
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Esperamos
	if(!WaitForSocket(simSocket,1))
		//Exit
		return 0;

	//Receive from everywhere
	memset(&from_addr, 0, from_len);
	//Set receiver IP address
	from_addr.sin_addr.s_addr = recIP;
	from_addr.sin_port = htons(recPort);

	//Leemos del socket
	int num = recvfrom(simSocket,recBuffer,MTU,0,(sockaddr*)&from_addr, &from_len);

	//If we don't have originating IP
	if (recIP==INADDR_ANY)
	{
		//Check if already NATED and got listener
		if (sendAddr.sin_addr.s_addr == INADDR_ANY && listener)
			//Request FIR
			listener->onFPURequested(this);
		//Bind it to first received packet ip
		recIP = from_addr.sin_addr.s_addr;
		//Get also port
		recPort = ntohs(from_addr.sin_port);
		//Log
		Log("-RTPSession NAT: received packet from [%s:%d]\n", inet_ntoa(from_addr.sin_addr), recPort);
	}

	//Increase stats
	numRecvPackets++;
	totalRecvBytes += num;

	//Comprobamos que nos haya devuelto algo
	if (num<12)
		//Exit
		return 0;
		
        //Vaciamos el paquete
        memset(data,0,*size);

	//Obtenemos las cabeceras
	rtp_hdr_t *headers = (rtp_hdr_t*)recBuffer;

	//Calculamos el inicio del paquete
	int ini = sizeof(rtp_hdr_t)-4;

	//Si hay campo cc
	if (headers->cc)
		ini += headers->cc;

	//Check if it still fits
	if (ini>num)
		//Exit
		return 0;

	//Get sec number
	WORD seq = ntohs(headers->seq);
	DWORD ssrc = ntohl(headers->ssrc);

	//If it is no first
	if (recSeq!=(WORD)-1 && recSSRC==ssrc)
	{
		//Check if it is an out of order packets
		if (seq<recSeq)
		{
			//Debug
			Debug("Video packet is out of order: last received=%d, seq num=%d.\n", recSeq, seq);
			//Ignore it
			return 0;
		}
		//Get the number of lost packets
		*lost = seq-(recSeq+1);
	} else {
		//Not lost
		*lost = 0;
	}

	//Update last one
	recSeq = seq;
	//Update ssrc
	recSSRC = ssrc;

	//Get the size of the headers without the csrc
	*size = num - ini;

	//Es el ultimo?
	*last = headers->m;

	//Obtenemos el tipo	
	BYTE type = headers->pt;

	//If we have a rtp map
	if (rtpVideoMapIn)
	{
		//Find the type in the map
		VideoCodec::RTPMap::iterator it = rtpVideoMapIn->find(type);
		//Check it is in there
		if (it!=rtpVideoMapIn->end())
			//It is our codec
			*codec = it->second;
		else
			//UUhh!!!
			return Error("RTP type %d unknown",type);
	} else {
		//Just convert it
		*codec = (VideoCodec::Type)type;
	}

	//Obtenemos el timestamp
	*timestamp = ntohl(headers->ts);

        //Copiamos al packete
        memcpy(data,recBuffer+ini,*size);

        //Y salimos
        return 1;	
}

/*********************************
* SendAudioPacket
*       Manda un packete de video
*********************************/
int RTPSession::SendAudioPacket(BYTE* data,int size,DWORD frameTime)
{
	//Check if we have sendinf ip address
	if (sendAddr.sin_addr.s_addr == INADDR_ANY)
	{
		//Do we have rec ip?
		if (recIP!=INADDR_ANY)
		{
			//Do NAT
			sendAddr.sin_addr.s_addr = recIP;
			//Get port
			sendAddr.sin_port = htons(recPort);
			//Log
			Log("-RTPSession NAT: Now sending to [%s:%d].\n", inet_ntoa(sendAddr.sin_addr), recPort);
		} else
			//Exit
			return 0;
	}
	
	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;

	//Incrementamos el numero de muestras
	sendTime+=frameTime;
	
	//POnemos el timestamp
	headers->ts=htonl(sendTime);

	//Incrementamos el numero de secuencia
	headers->seq=htons(sendSeq++);

	//La marca de fin de frame
	headers->m=0;

	//Calculamos el inicio
	int ini= sizeof(rtp_hdr_t) -4;

	//Si tiene cc
	if (headers->cc)
		ini+=4;

	//Comprobamos que quepan
	if (ini+size>MTU)
		return 0;

	//Copiamos los datos
	memcpy(sendPacket+ini,data,size);

	//Mandamos el mensaje
	int ret = sendto(simSocket,sendPacket,size+ini,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Inc stats
	numSendPackets++;
	totalSendBytes += size;

	return (ret>0); 
}

/*********************************
* GetAudioPacket
*	Lee el siguiente paquete de video
*********************************/
int RTPSession::GetAudioPacket(BYTE *data,DWORD *size,BYTE *lost,AudioCodec::Type *codec,DWORD *timestamp)
{
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Esperamos
        if(!WaitForSocket(simSocket,1))
		// Exit
		return 0;

	//Receive from everywhere
	memset(&from_addr, 0, from_len);
	//Set receiver IP address
	from_addr.sin_addr.s_addr = recIP;
	from_addr.sin_port = htons(recPort);

	//Leemos del socket
	int num = recvfrom(simSocket,recBuffer,MTU,0,(sockaddr*)&from_addr, &from_len);

	//If we don't have originating IP
	if (recIP==INADDR_ANY)
	{
		//Bind it to first received packet ip
		recIP = from_addr.sin_addr.s_addr;
		//Get also port
		recPort = ntohs(from_addr.sin_port);
		//Log
		Log("-RTPSession NAT: received packet from [%s:%d].\n", inet_ntoa(from_addr.sin_addr), recPort);
	}
	
	//Increase stats
	numRecvPackets++;
	totalRecvBytes += num;

	//Comprobamos que nos haya devuelto algo
	if (num<12)
		return Error("Audio packet too small [%d]\n",num);

        //Vaciamos el paquete
        memset(data,0,*size);

	//Obtenemos las cabeceras
	rtp_hdr_t *headers = (rtp_hdr_t*)recBuffer;

	//Calculamos el inicio del paquete
	int ini = sizeof(rtp_hdr_t)-4;

	//Si hay campo cc
	if (headers->cc)
		ini += headers->cc;

	//Check if it still fits
	if (ini>num)
		//Exit
		return Error("Audio packet too small [%d]\n",num);
	
	//Obtenemos el tama�o de las headers sin el csrc
	if (*size <  num - ini)
		return Error("Not enought size for audio data[size:%d,data:%d]\n",*size,num-ini);
	else
		*size = num - ini;

	//Get sec number
	WORD seq = ntohs(headers->seq);
        //Get ssrc
        DWORD ssrc = ntohl(headers->ssrc);

	//If it is no first
	if (recSeq!=((WORD)-1) && recSSRC==ssrc)
	{
		//Check if it is an out of order packets
		if (seq<recSeq)
		{
			//Debug
			Log("Audio packet is out of order: last received=%d, seq num=%d.\n", recSeq, seq);
			//Ignore it
			return 0;
		}
		//Get the number of lost packets
		*lost = seq-(recSeq+1);
	} else {
		//Not lost
		*lost = 0;
	}

	//Update last one
	recSeq = seq;
	//Update ssrc
	recSSRC = ssrc;

	//Obtenemos el tipo
	BYTE type = headers->pt;

	//If we have a rtp map
	if (rtpAudioMapIn)
	{
		//Find the type in the map
		AudioCodec::RTPMap::iterator it = rtpAudioMapIn->find(type);
		//Check it is in there
		if (it!=rtpAudioMapIn->end())
			//It is our codec
			*codec = it->second;
		else
			//UUhh!!!
			return Error("RTP type %d unknown",type);
	} else {
		//Just convert it
		*codec = (AudioCodec::Type)type;
	};

	//Obtenemos el timestamp
	*timestamp = ntohl(headers->ts);

	//Actualizamos el anterior
	recSeq = ntohs(headers->seq);

        //Copiamos al packete
        memcpy(data,recBuffer+ini,*size);

        //Y salimos
        return 1;	
}

/*********************************
* SendTextPacket
*       Manda un packete de texto
*********************************/
int RTPSession::SendTextPacket(BYTE* data,int size,DWORD timastamp,bool mark)
{
	//Check if we have sendinf ip address
	if (sendAddr.sin_addr.s_addr == INADDR_ANY)
	{
		//Do we have rec ip?
		if (recIP!=INADDR_ANY)
		{
			//Do NAT
			sendAddr.sin_addr.s_addr = recIP;
			//Get port
			sendAddr.sin_port = htons(recPort);
		} else
			//Exit
			return 0;
	}
	
	//Modificamos las cabeceras del packete
	rtp_hdr_t *headers = (rtp_hdr_t *)sendPacket;

	//Incrementamos el numero de muestras
	sendTime = timastamp;

	//POnemos el timestamp
	headers->ts=htonl(timastamp);

	//Incrementamos el numero de secuencia
	headers->seq=htons(sendSeq++);

	//La marca de fin de frame
	headers->m=mark;

	//Calculamos el inicio
	int ini= sizeof(rtp_hdr_t) -4;

	//Si tiene cc
	if (headers->cc)
		ini+=4;

	//Comprobamos que quepan
	if (ini+size>MTU)
		return 0;

	//Copiamos los datos
	memcpy(sendPacket+ini,data,size);

	//Mandamos el mensaje
	int ret = sendto(simSocket,sendPacket,size+ini,0,(sockaddr *)&sendAddr,sizeof(struct sockaddr_in));

	//Inc stats
	numSendPackets++;
	totalSendBytes += size;
	
	return (ret>0);
}

/*********************************
* GetTextPacket
*	Lee el siguiente paquete de video
*********************************/
int RTPSession::GetTextPacket(BYTE *data,DWORD *size,BYTE *lost,TextCodec::Type *codec,DWORD *timestamp)
{
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Esperamos
        if(!WaitForSocket(simSocket,1))
		// Exit
		return 0;

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Set receiver IP address
	from_addr.sin_addr.s_addr = recIP;
	from_addr.sin_port = htons(recPort);

	//Leemos del socket
	int num = recvfrom(simSocket,recBuffer,MTU,0,(sockaddr*)&from_addr, &from_len);

	//If we don't have originating IP
	if (recIP==INADDR_ANY)
	{
		//Bind it to first received packet ip
		recIP = from_addr.sin_addr.s_addr;
		//Get also port
		recPort = ntohs(from_addr.sin_port);
		//Log
		Log("-RTPSession NAT: received packet from [%s:%d]\n", inet_ntoa(from_addr.sin_addr), recPort);
	}

	//Increase stats
	numRecvPackets++;
	totalRecvBytes += num;

	//Comprobamos que nos haya devuelto algo
	if (num<12)
		return Error("Text packet too small [%d]\n",num);

        //Vaciamos el paquete
        memset(data,0,*size);

	//Obtenemos las cabeceras
	rtp_hdr_t *headers = (rtp_hdr_t*)recBuffer;

	//Calculamos el inicio del paquete
	int ini = sizeof(rtp_hdr_t)-4;

	//Si hay campo cc
	if (headers->cc)
		ini += headers->cc;

	//Check if it still fits
	if (ini>num)
		//Exit
		return Error("Text packet too small [%d]\n",num);
	
	//Get the size of the headers without csrc
	if (*size <  num - ini)
		return Error("Not enought size for text data[size:%d,data:%d]\n",*size,num-ini);
	else
		*size = num - ini;

	//Get sec number
	WORD seq = ntohs(headers->seq);

	//Get ssrc
	DWORD ssrc = ntohl(headers->ssrc);

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
					//Ignore it
					return 0;
				else if (seq==1 && recSeq==(WORD)-1)
					//Not lost
					*lost = 0;
				else
					//Too many
					*lost = (BYTE)-1;
			} else if (seq-(recSeq+1)<255) {
				//Get the number of lost packets
				*lost = seq-(recSeq+1);
			} else {
				//Too much
				*lost = (BYTE)-1;
			}
		} else {
			//Not lost
			*lost = 0;
		}
	} else {
		//Lost
		*lost = (BYTE)-1;
	}

	//Update ssrc
	recSSRC = ssrc;

	//Update last one
	recSeq = seq;

	//Obtenemos el tipo
	BYTE type = headers->pt;

	//If we have a rtp map
	if (rtpTextMapIn)
	{
		//Find the type in the map
		TextCodec::RTPMap::iterator it = rtpTextMapIn->find(type);
		//Check it is in there
		if (it!=rtpTextMapIn->end())
			//It is our codec
			*codec = it->second;
		else
			//UUhh!!!
			return Error("RTP type %d unknown",type);
	} else {
		//Just convert it
		*codec = (TextCodec::Type)type;
	}

	//Obtenemos el timestamp
	*timestamp = ntohl(headers->ts);

        //Copiamos al packete
        memcpy(data,recBuffer+ini,*size);

        //Y salimos
        return 1;
}
