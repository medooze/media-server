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
#include <string>
#include <openssl/ossl_typ.h>
#include "log.h"
#include "assertions.h"
#include "tools.h"
#include "rtp.h"
#include "stunmessage.h"
#include "RTPBundleTransport.h"
#include "RTPTransport.h"
#include "ICERemoteCandidate.h"


/*************************
* RTPBundleTransport
* 	Constructro
**************************/
RTPBundleTransport::RTPBundleTransport()
{
	//Init values
	socket = FD_INVALID;
	port = 0;
	
	//No thread
	setZeroThread(&thread);
	running = false;
}

/*************************
* ~RTPBundleTransport
* 	Destructor
**************************/
RTPBundleTransport::~RTPBundleTransport()
{
	Debug("-RTPBundleTransport::~RTPBundleTransport()\n");
	//End)
	End();
}

DTLSICETransport* RTPBundleTransport::AddICETransport(const std::string &username,const Properties& properties)
{
	Log("-RTPBundleTransport::AddICETransport [%s]\n",username.c_str());
	
	Properties ice;
	Properties dtls;
		
	//Get child properties
	properties.GetChildren("ice",ice);
	properties.GetChildren("dtls",dtls);
	
	//Ensure that we have required ICE properties
	if (!ice.HasProperty("remoteUsername") || !ice.HasProperty("remotePassword") || !ice.HasProperty("localUsername") || !ice.HasProperty("localPassword"))
	{
		//Error
		Error("-Missing ICE properties\n");
		//Error
		return NULL;
	}
	
	//Ensure that we have required DTLS properties
	if (!dtls.HasProperty("setup") || !dtls.HasProperty("fingerprint") || !dtls.HasProperty("hash"))
	{
		//Error
		Error("-Missing DTLS properties\n");
		//Error
		return NULL;
	}
	
	
	//Create new ICE transport
	DTLSICETransport *transport = new DTLSICETransport(this);
	
	//Set local STUN properties
	transport->SetLocalSTUNCredentials(ice.GetProperty("localUsername"),ice.GetProperty("localPassword"));
	transport->SetRemoteSTUNCredentials(ice.GetProperty("remoteUsername"),ice.GetProperty("remotePassword"));
	
	//Set remote DTLS 
	transport->SetRemoteCryptoDTLS(dtls.GetProperty("setup"),dtls.GetProperty("hash"),dtls.GetProperty("fingerprint"));
	
	//Synchronized
	{
		//Lock scope
		ScopedUseLock scope(use);
		//Add it
		connections[username] = new Connection(transport);
	}
	
	
	//OK
	return transport;
}

int RTPBundleTransport::RemoveICETransport(const std::string &username)
{
	Log("-RTPBundleTransport::RemoveICETransport() [username:%s]\n",username.c_str());
  
	//Lock method
	ScopedUseLock scope(use);
		
	//Get transport
	auto it = connections.find(username);
	
	//Check
	if (it==connections.end())
		//Error
		return Error("-ICE transport not found\n");
	
	//Get connection 
	Connection* connection = it->second;
	
	//REmove connection
	connections.erase(it);
	
	//Get all candidates
	for( auto it2=connection->candidates.begin(); it2!=connection->candidates.end(); ++it2)
	{
		//Get candidate object
		ICERemoteCandidate* candidate = *it2;
		//Create remote string
		char remote[24];
		snprintf(remote,sizeof(remote),"%s:%hu", candidate->GetIP(),candidate->GetPort());
		//Remove from all candidates list
		candidates.erase(std::string(remote));
		//Delete candidate
		delete(candidate);
	}
	
	//Delete connection wrapper and transport
	delete(connection->transport);
	delete(connection);
		
	//DOne
	return 1;
}

/********************************
* Init
*	Inicia el control rtcp
********************************/
int RTPBundleTransport::Init()
{
	int retries = 0;

	Log(">RTPBundleTransport::Init()\n");

	sockaddr_in recAddr;

	//Clear addr
	memset(&recAddr,0,sizeof(struct sockaddr_in));

	//Set family
	recAddr.sin_family     	= AF_INET;

	//Get two consecutive ramdom ports
	while (retries++<100)
	{
		//If we have a rtp socket
		if (socket!=FD_INVALID)
		{
			// Close first socket
			MCU_CLOSE(socket);
			//No socket
			socket = FD_INVALID;
		}

		//Create new sockets
		socket = ::socket(PF_INET,SOCK_DGRAM,0);
		//Get random
		port = (RTPTransport::GetMinPort()+(RTPTransport::GetMaxPort()-RTPTransport::GetMinPort())*double(rand()/double(RAND_MAX)));
		//Make even
		port &= 0xFFFFFFFE;
		//Try to bind to port
		recAddr.sin_port = htons(port);
		//Bind the rtp socket
		if(bind(socket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
			//Try again
			continue;
		//Set COS
		int cos = 5;
		setsockopt(socket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
		//Set TOS
		int tos = 0x2E;
		setsockopt(socket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
		//Everything ok
		Log("-RTPBundleTransport::Init() | Got port [%d]\n",port);
		//Start receiving
		Start();
		//Done
		Log("<RTPBundleTransport::Init()\n");
		//Opened
		return port;
	}

	//Error
	Error("-RTPBundleTransport::Init() | too many failed attemps opening sockets\n");

	//Failed
	return 0;
}

/*********************************
* End
*	Termina la todo
*********************************/
int RTPBundleTransport::End()
{
	//Check if not running
	if (!running)
		//Nothing
		return 0;

	Log(">RTPBundleTransport::End()\n");

	//Stop just in case
	Stop();

	//Not running;
	running = false;
	//If got socket
	if (socket!=FD_INVALID)
	{
		//Will cause poll to return
		MCU_CLOSE(socket);
		//No sockets
		socket = FD_INVALID;
	}

	Log("<RTPBundleTransport::End()\n");

	return 1;
}

int RTPBundleTransport::Send(const ICERemoteCandidate* candidate,const BYTE *buffer,const DWORD size)
{
	//Send packet
	return sendto(socket,buffer,size,MSG_DONTWAIT,candidate->GetAddress(),candidate->GetAddressLen());
}

/*********************************
* GetTextPacket
*	Lee el siguiente paquete de video
*********************************/
int RTPBundleTransport::Read()
{
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	DWORD size = MTU;
	sockaddr_in from_addr;
	DWORD from_len = sizeof(from_addr);
	
	//Inc usage
	ScopedUse scope(use);

	//Receive from everywhere
	memset(&from_addr, 0, from_len);

	//Leemos del socket
	int len = recvfrom(socket,data,MTU,MSG_DONTWAIT,(sockaddr*)&from_addr, &from_len);
	
	// Ignore empty datagrams and errors
	if (len <= 0)
		return 0;
	
	//Get remote ip:port address
	char remote[24];
	snprintf(remote,sizeof(remote),"%s:%hu", inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));
				
	//Get candidate
	std::string candidate(remote);

	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(data,len))
	{
		//Parse it
		STUNMessage *stun = STUNMessage::Parse(data,len);

		//It was not a valid STUN message
		if (!stun)
			//Error
			return Error("-RTPBundleTransport::ReadRTP() | failed to parse STUN message\n");

		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();

		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			//Get username
			STUNMessage::Attribute* attr = stun->GetAttribute(STUNMessage::Attribute::Username);

			//Copy username string
			std::string username((char*)attr->attr,attr->size);
			
			//Check if we have an ICE transport for that username
			auto it = connections.find(username);
			
			//If not found
			if (it==connections.end())
				//TODO: Reject
				//Exit
				return Error("-RTPBundleTransport::Read ice username not found [%s}\n",username.c_str());
			
			//Get ice connection
			Connection* connection = it->second;
			DTLSICETransport* transport = connection->transport;
			ICERemoteCandidate* candidate = NULL;

			//Check if it has the prio attribute
			if (!stun->HasAttribute(STUNMessage::Attribute::Priority))
				//Error
				return Error("-STUN Message without priority attribute");
			
			//Get attribute
			STUNMessage::Attribute* priority = stun->GetAttribute(STUNMessage::Attribute::Priority);
			
			//Get prio
			DWORD prio = get4(priority->attr,0);
			
			//Find candidate
			auto it2 = candidates.find(remote);
			
			//Check if it is not already present
			if (it2==candidates.end())
			{
				//Create new candidate
				ICERemoteCandidate* candidate = new ICERemoteCandidate(inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),transport);
				//Add candidate and add it to the maps
				candidates[remote] = candidate;
				connection->candidates.insert(candidate);
			} else {
				//Get it
				candidate = it2->second;
			}
			
			//Set it active
			transport->ActivateRemoteCandidate(candidate,stun->HasAttribute(STUNMessage::Attribute::UseCandidate),prio);
			
			//Create response
			STUNMessage* resp = stun->CreateResponse();
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(&from_addr);
			
			//Serialize and autenticate
			len = resp->AuthenticatedFingerPrint(data,size,transport->GetLocalPwd());

			//Send response
			sendto(socket,data,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

			//Clean response
			delete(resp);
			
			len = 0;
			//Create trans id
			BYTE transId[12];
			//Set first to 0
			set4(transId,0,0);
			//Set timestamp as trans id
			set8(transId,4,getTime());
			//Create binding request to send back
			STUNMessage *request = new STUNMessage(STUNMessage::Request,STUNMessage::Binding,transId);
			//Add username
			request->AddUsernameAttribute(transport->GetLocalUsername(),transport->GetRemoteUsername());
			//Add other attributes
			request->AddAttribute(STUNMessage::Attribute::IceControlled,(QWORD)1);
			request->AddAttribute(STUNMessage::Attribute::Priority,(DWORD)33554431);

			//Serialize and autenticate
			len = request->AuthenticatedFingerPrint(data,size,transport->GetRemotePwd());

			//Send it
			sendto(socket,data,len,0,(sockaddr *)&from_addr,sizeof(struct sockaddr_in));

			//Clean response
			delete(request);
		}

		//Delete message
		delete(stun);

		//Exit
		return 1;
	}
	
	//Find candidate
	auto it = candidates.find(remote);
	
	//Check if it was not registered
	if (it==candidates.end())
		//Error
		return Error("-No registered ICE candidate for [%s]",remote);
	
	//Get ice transport
	ICERemoteCandidate *ice = it->second;
	
	//Send data
	ice->onData(data,len);
	
	//OK
	return 1;
}

void RTPBundleTransport::Start()
{
	//We are running
	running = true;

	//Create thread
	createPriorityThread(&thread,run,this,0);
}

void RTPBundleTransport::Stop()
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
void * RTPBundleTransport::run(void *par)
{
	Log("-RTPBundleTransport::run() | thread [%d,0x%x]\n",getpid(),par);

	//Block signals to avoid exiting on SIGUSR1
	blocksignals();

        //Obtenemos el parametro
        RTPBundleTransport *sess = (RTPBundleTransport *)par;

        //Ejecutamos
        sess->Run();
	
	//Exit
	return NULL;
}

/***************************
 * Run
 * 	Server running thread
 ***************************/
int RTPBundleTransport::Run()
{
	Log(">RTPBundleTransport::Run() | [%p]\n",this);

	//Set values for polling
	ufds[0].fd = socket;
	ufds[0].events = POLLIN | POLLERR | POLLHUP;

	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(socket,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	fcntl(socket,F_SETFL,fsflags);
	
	//Catch all IO errors
	signal(SIGIO,EmptyCatch);

	//Run until ended
	while(running)
	{
		//Wait for events
		if(poll(ufds,sizeof(ufds)/sizeof(pollfd),-1)<0)
			//Check again
			continue;

		if (ufds[0].revents & POLLIN)
			//Read rtp data
			Read();

		if ((ufds[0].revents & POLLHUP) || (ufds[0].revents & POLLERR))
		{
			//Error
			Log("-RTPBundleTransport::Run() | Pool error event [%d]\n",ufds[0].revents);
			//Exit
			break;
		}
	}

	Log("<RTPBundleTransport::Run()\n");
	
	return 0;
}


