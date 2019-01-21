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
#include <openssl/opensslconf.h>
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
	//Mutex
	pthread_mutex_init(&mutex,0);
	pthread_cond_init(&cond,0);
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
	//Mutex
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

// 根据username创建connection及transport对象

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
	
	//Set SRTP protection profiles
	std::string profiles = properties.GetProperty("srtpProtectionProfiles","");
	transport->SetSRTPProtectionProfiles(profiles);
	
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
		connections[username] = new Connection(transport, properties.GetProperty("disableSTUNKeepAlive", false));
	}
	
	//Start it
	transport->Start();
	
	//OK
	return transport;
}

// 根据username删除connection、transport和所有的candidates对象

int RTPBundleTransport::RemoveICETransport(const std::string &username)
{
	Log("-RTPBundleTransport::RemoveICETransport() [username:%s]\n",username.c_str());
  
	//Lock
	use.WaitUnusedAndLock();

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
	
	//Unlock
	use.Unlock();
	
	//Stop transport
	connection->transport->Stop();
	
	//Delete connection wrapper and transport
	delete(connection->transport);
	delete(connection);
		
	//DOne
	return 1;
}

// 1. 绑定端口号，打开sokcet服务
// 2. 启动线程开始读取rtp数据包

int RTPBundleTransport::Init()
{
	int retries = 0;

	Log(">RTPBundleTransport::Init()\n");

	sockaddr_in recAddr;

	//Clear addr
	memset(&recAddr,0,sizeof(struct sockaddr_in));
	//Init ramdon
	srand (time(NULL));

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
#ifdef SO_PRIORITY
		//Set COS
		int cos = 5;
		setsockopt(socket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
#endif
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

int RTPBundleTransport::Init(int port)
{
	if (!port)
		return Init();
	
	Log(">RTPBundleTransport::Init(%d)\n",port);

	sockaddr_in recAddr;

	//Clear addr
	memset(&recAddr,0,sizeof(struct sockaddr_in));
	//Init ramdon
	srand (time(NULL));

	//Set family
	recAddr.sin_family     	= AF_INET;

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
	//Try to bind to port
	recAddr.sin_port = htons(port);
	//Bind the rtp socket
	if(bind(socket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
		//Error
		return Error("-RTPBundleTransport::Init() | could not open port\n");
	
#ifdef SO_PRIORITY
	//Set COS
	int cos = 5;
	setsockopt(socket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
#endif
	//Set TOS
	int tos = 0x2E;
	setsockopt(socket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
	//Everything ok
	Log("-RTPBundleTransport::Init() | Got port [%d]\n",port);
	//Store local port
	this->port = port;
	//Start receiving
	Start();
	//Done
	Log("<RTPBundleTransport::Init()\n");
	//Opened
	return port;
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


/**
	Send Function:
	1. 这个函数是对candidate的接口发送数据的接口的实现
**/

int RTPBundleTransport::Send(const ICERemoteCandidate* candidate,const BYTE *buffer,const DWORD size)
{
	int ret = 0;
		
	//Send packet
	while(running && (ret=sendto(socket,buffer,size,MSG_DONTWAIT,candidate->GetAddress(),candidate->GetAddressLen()))<0)
	{
		//Check error
		if (errno!=EAGAIN)
		{
			Error("-RTPBundleTransport::Send error [errno:%d,size:%d]\n",errno,size);
			//Error
			break;
		}
		
		UltraDebug("->RTPBundleTransport::Send() | retry \n");
		
		//Lock
		pthread_mutex_lock(&mutex);
	
		//Get now
		timespec ts;
		timeval tp;
		gettimeofday(&tp, NULL);

		//Calculate 5ms timeout at most
		calcAbsTimeout(&ts,&tp,5);
		
		//Set to wait also for read events
		ufds[0].events = POLLIN | POLLOUT | POLLERR | POLLHUP;

		//Check thred
		if (!isZeroThread(thread))
			//Signal the pthread this will cause the poll call to exit
			pthread_kill(thread,SIGIO);
		
		//Wait next or stopped
		pthread_cond_timedwait(&cond,&mutex,&ts);
		
		//Don't wait for write evetns
		ufds[0].events = POLLIN | POLLERR | POLLHUP;
	
		//Unlock
		pthread_mutex_unlock(&mutex);
		
		UltraDebug("<RTPBundleTransport::Send() | retry\n");
	}
	
	//Done
	return  ret;
}

/*
	Read fuction：
	1. 是RTPBundleTransport的模块接收函数，负责接收来自其他client的udp数据包，包括stun包和rtcp+rtp包等。
	2. 负责将接收到的数据包，负责分发给其他class模块处理: 
	  	(2.1) stun 相关的消息，自己处理。
	  	(2.2) rtcp+rtp 相关的数据包交给 DTLSICETransport模块处理。 
*/

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
			
			//Check wether we have to reply to this message or not
			bool reply = !connection->disableSTUNKeepAlive;
			
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
				candidate = new ICERemoteCandidate(inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),transport);
				//Add candidate and add it to the maps
				candidates[remote] = candidate;
				connection->candidates.insert(candidate);
				//We need to reply the first always
				reply = true;
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
			
			//If the STUN keep alive response is not disabled
			if (reply)
			{
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
		return Error("-RTPBundleTransport::ReadRTP() | No registered ICE candidate for [%s]\n",remote);
	
	//Get ice transport
	ICERemoteCandidate *ice = it->second;
	
	//Send data
	ice->onData(data,len);
	
	//OK
	return 1;
}

int RTPBundleTransport::AddRemoteCandidate(const std::string& username,const char* ip, WORD port)
{
	BYTE data[MTU+SRTP_MAX_TRAILER_LEN] ZEROALIGNEDTO32;
	DWORD size = MTU;
	DWORD len = 0;
	
	//Inc usage
	ScopedUse scope(use);
	
	//Get remote ip:port address
	char remote[strlen(ip)+6];
	snprintf(remote,sizeof(remote),"%s:%hu",ip, port);
				
	//Check if we have an ICE transport for that username
	auto it = connections.find(username);
			
	//If not found
	if (it==connections.end())
		//Exit
		return Error("-RTPBundleTransport::AddRemoteCandidate:: ice username not found [%s}\n",username.c_str());

	//Get ice connection
	Connection* connection = it->second;
	DTLSICETransport* transport = connection->transport;

	//Check if it is not already present
	if (candidates.find(remote)!=candidates.end())
		//Do nothing
		return Error("-RTPBundleTransport::AddRemoteCandidate already present [candidate:%s]\n",remote);
	
	//Create new candidate
	ICERemoteCandidate* candidate = new ICERemoteCandidate(ip,port,transport);
	//Add candidate and add it to the maps
	candidates[remote] = candidate;
	connection->candidates.insert(candidate);

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
	sendto(socket,data,len,MSG_DONTWAIT,candidate->GetAddress(),candidate->GetAddressLen());

	//Clean request
	delete(request);
	
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
	//Signal cancel
	pthread_cond_signal(&cond);
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
		
		if (ufds[0].revents & POLLOUT)
			//Signal write enable
			pthread_cond_signal(&cond);

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

