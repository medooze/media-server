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
#include "log.h"
#include "tools.h"
#include "rtp.h"
#include "rtpsession.h"

BYTE rtpEmpty[] = {0x80,0x14,0x00,0x00,0x00,0x00,0x00,0x00};

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
	sendSSRC = random();
	recSeq = (WORD)-1;
	recSSRC = 0;
	recIP = INADDR_ANY;
	recPort = 0;
	//Empty types by defauilt
	rtpMapIn = NULL;
	rtpMapOut = NULL;
	//Empty stats
	numRecvPackets = 0;
	numSendPackets = 0;
	totalRecvBytes = 0;
	totalSendBytes = 0;
	lostRecvPackets = 0;
	lost = 0;
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
	if (rtpMapIn)
		delete(rtpMapIn);
	if (rtpMapOut)
		delete(rtpMapOut);
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
	//Stop just in case
	Stop();

	//If running
	if (thread)
	{
		//Wait for server thread to close
		pthread_join(thread,NULL);
		//No thread
		thread = NULL;
	}

	return 1;
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
		Log("SendPacket Overflow\n");
		return 0;
	}

	//Copiamos los datos
        memcpy(sendPacket+ini,packet.GetMediaData(),packet.GetMediaLength());
	
	//Set pateckt length
	DWORD len = packet.GetMediaLength()+ini;

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

}
/*********************************
* GetTextPacket
*	Lee el siguiente paquete de video
*********************************/
void RTPSession::ReadRTP()
{
	RTPPacket *packet = new RTPPacket(media,0,0);
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Set receiver IP address
	from_addr.sin_addr.s_addr = recIP;
	from_addr.sin_port = htons(recPort);

	//Leemos del socket
	int num = recvfrom(simSocket,packet->GetData(),packet->GetMaxSize(),0,(sockaddr*)&from_addr, &from_len);

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
		return;

	//Set size
	packet->SetSize(num);
	
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
					//Ignore it
					return;
				else if (seq==1 && recSeq==(WORD)-1)
					//Not lost
					lost = 0;
				else
					//Too many
					lost = (WORD)-1;
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
	} else {
		//Lost
		lost = (WORD)-1;
	}

	//Check if it has not been something extrange
	if (lost!=(WORD)-1)
		//Increase lost packet count
		lostRecvPackets += lost;

	//Update ssrc
	recSSRC = ssrc;

	//Update last one
	recSeq = seq;

	//Obtenemos el tipo
	BYTE type = packet->GetType();
	
	//Check maps
	if (rtpMapIn)
	{
		//Set media

		//Find the type in the map
		RTPMap::iterator it = rtpMapIn->find(type);
		//Check it is in there
		if (it!=rtpMapIn->end())
			//It is our codec
			packet->SetCodec(it->second);
	} else
		//Set codec
		packet->SetCodec(type);

	//Push it back
	packets.Add(packet);
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
		//Signal the pthread this will cause the poll call to exit
		pthread_kill(thread,SIGIO);
		//Wait thread to close
		pthread_join(thread,NULL);
	}

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
	ufds[0].events = POLLIN | POLLERR | POLLHUP;

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
	//Send it
	return packets.Pop();
}
