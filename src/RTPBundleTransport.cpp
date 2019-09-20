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
#include "EventLoop.h"

/*************************
* RTPBundleTransport
* 	Constructro
**************************/
RTPBundleTransport::RTPBundleTransport() : 
	loop(this)
{
	//Init values
	socket = FD_INVALID;
	port = 0;
}

/*************************
* ~RTPBundleTransport
* 	Destructor
**************************/
RTPBundleTransport::~RTPBundleTransport()
{
	//End)
	End();
}

RTPBundleTransport::Connection* RTPBundleTransport::AddICETransport(const std::string &username,const Properties& properties)
{
	Log("-RTPBundleTransport::AddICETransport() | [%s]\n",username.c_str());
	
	Properties ice;
	Properties dtls;
		
	//Get child properties
	properties.GetChildren("ice",ice);
	properties.GetChildren("dtls",dtls);
	
	//Ensure that we have required ICE properties
	if (!ice.HasProperty("remoteUsername") || !ice.HasProperty("remotePassword") || !ice.HasProperty("localUsername") || !ice.HasProperty("localPassword"))
	{
		//Error
		Error("-RTPBundleTransport::AddICETransport() | Missing ICE properties\n");
		//Error
		return NULL;
	}
	
	//Ensure that we have required DTLS properties
	if (!dtls.HasProperty("setup") || !dtls.HasProperty("fingerprint") || !dtls.HasProperty("hash"))
	{
		//Error
		Error("-RTPBundleTransport::AddICETransport() | Missing DTLS properties\n");
		//Error
		return NULL;
	}
	
	//Create new ICE transport
	DTLSICETransport *transport = new DTLSICETransport(this,loop);
	
	//Set SRTP protection profiles
	std::string profiles = properties.GetProperty("srtpProtectionProfiles","");
	transport->SetSRTPProtectionProfiles(profiles);
	
	//Set local STUN properties
	transport->SetLocalSTUNCredentials(ice.GetProperty("localUsername"),ice.GetProperty("localPassword"));
	transport->SetRemoteSTUNCredentials(ice.GetProperty("remoteUsername"),ice.GetProperty("remotePassword"));
	
	//Set remote DTLS 
	transport->SetRemoteCryptoDTLS(dtls.GetProperty("setup"),dtls.GetProperty("hash"),dtls.GetProperty("fingerprint"));
	
	//Create connection
	auto connection = new Connection(username,transport,properties.GetProperty("disableSTUNKeepAlive", false));
	
	//Synchronized
	loop.Async([=](...){
		//Add it
		connections[username] = connection;
		//Start it
		transport->Start();
	});
	
	//OK
	return connection;
}

int RTPBundleTransport::RemoveICETransport(const std::string &username)
{
	Log("-RTPBundleTransport::RemoveICETransport() [username:%s]\n",username.c_str());
  
	//Synchronized
	loop.Async([this,username](...){

		//Get transport
		auto connectionIterator = connections.find(username);

		//Check
		if (connectionIterator==connections.end())
		{
			//Error
			Error("-RTPBundleTransport::RemoveICETransport() | ICE transport not found\n");
			//Done
			return;
		}

		//Get connection 
		Connection* connection = connectionIterator->second;

		//REmove connection
		connections.erase(connectionIterator);

		//Get all candidates
		for( auto candidatesIterator=connection->candidates.begin(); candidatesIterator!=connection->candidates.end(); ++candidatesIterator)
		{
			//Get candidate object
			ICERemoteCandidate* candidate = *candidatesIterator;
			//Remove from all candidates list
			candidates.erase(candidate->GetRemoteAddress());
		}
	
		//Stop transport
		connection->transport->Stop();

		//Delete connection wrapper and transport
		delete(connection->transport);
		delete(connection);
	});

	//DOne
	return 1;
}

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
		//Try to bind to port
		recAddr.sin_port = htons(port);
		//Bind the rtp socket
		if(bind(socket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
		{
			Log("-could not bind");
			//Try again
			continue;
		}
		//If port was random
		if (!port)
		{
			socklen_t len = sizeof(struct sockaddr_in);
			//Get binded port
			if (getsockname(socket,(struct sockaddr *)&recAddr,&len)!=0)
				//Try again
				continue;
			//Get final port
			port = ntohs(recAddr.sin_port);
		}
#ifdef SO_PRIORITY
		//Set COS
		int cos = 5;
		setsockopt(socket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
#endif
		//Set TOS
		int tos = 0x2E;
		setsockopt(socket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
		
#ifdef IP_PMTUDISC_DONT			
		//Disable path mtu discoveruy
		int pmtu = IP_PMTUDISC_DONT;
		setsockopt(socket, IPPROTO_IP, IP_MTU_DISCOVER, &pmtu, sizeof(pmtu));
#endif
		
		//Everything ok
		Log("-RTPBundleTransport::Init() | Got port [%d]\n",port);
		//Start receiving
		loop.Start(socket);
		//Create ice timer
		iceTimer = loop.CreateTimer([=](std::chrono::milliseconds now){ this->onTimer(now); });
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
	
#ifdef IP_PMTUDISC_DONT	
	//Disable path mtu discoveruy
	int pmtu = IP_PMTUDISC_DONT;
	setsockopt(socket, IPPROTO_IP, IP_MTU_DISCOVER, &pmtu, sizeof(pmtu));
#endif
	//Everything ok
	Log("-RTPBundleTransport::Init() | Got port [%d]\n",port);
	//Store local port
	this->port = port;
	//Start receiving
	loop.Start(socket);
	
	//Create ice timer
	iceTimer = loop.CreateTimer([=](std::chrono::milliseconds now){ this->onTimer(now); });
	
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
	//Check we are already running
	if (!loop.IsRunning())
		return 0;
	
	Log(">RTPBundleTransport::End()\n");
	
	//Stop timer
	if (iceTimer)
		//Cancel it
		iceTimer->Cancel();

	//Stop loop
	loop.Stop();

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

int RTPBundleTransport::Send(const ICERemoteCandidate* candidate, Packet&& buffer)
{
	loop.Send(candidate->GetIPAddress(),candidate->GetPort(),std::move(buffer));
	return 1;
}

void RTPBundleTransport::OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ip, const uint16_t port)
{
	//Get remote ip:port address
	std::string remote = ICERemoteCandidate::GetRemoteAddress(ip,port);
	
	//UltraDebug("-RTPBundleTransport::OnRead() | [remote:%s,size:%u]\n",remote.c_str(),size);
			
	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(data,size))
	{
		//UltraDebug("-RTPBundleTransport::OnRead() | stun\n");
		
		//Parse it
		auto stun = std::unique_ptr<STUNMessage>(STUNMessage::Parse(data,size));

		//It was not a valid STUN message
		if (!stun)
		{
			//Error
			Error("-RTPBundleTransport::Read() | failed to parse STUN message\n");
			//Done
			return;
		}

		STUNMessage::Type type = stun->GetType();
		STUNMessage::Method method = stun->GetMethod();

		//If it is a request
		if (type==STUNMessage::Request && method==STUNMessage::Binding)
		{
			//UltraDebug("-RTPBundleTransport::OnRead() | Binding request\n");
			
			//Check if it has the prio attribute
			if (!stun->HasAttribute(STUNMessage::Attribute::Username))
			{
				//Error
				Debug("-RTPBundleTransport::Read() | STUN Message without username attribute\n");
				//DOne
				return;
			}
			
			//Get username
			STUNMessage::Attribute* attr = stun->GetAttribute(STUNMessage::Attribute::Username);
			
			//Copy username string
			std::string username((char*)attr->attr,attr->size);
			
			//Check if we have an ICE transport for that username
			auto it = connections.find(username);
			
			//If not found
			if (it==connections.end())
			{
				//TODO: Reject
				//Error
				Debug("-RTPBundleTransport::Read() | ICE username not found [%s}\n",username.c_str());
				//Done
				return;
			}
			
			//Get ice connection
			Connection* connection = it->second;
			DTLSICETransport* transport = connection->transport;
			
			//Authenticate request with remote username
			if (!stun->CheckAuthenticatedFingerPrint(data,size,transport->GetLocalPwd()))
			{
				//Error
				Error("-RTPBundleTransport::Read() | STUN Message request failed authentication [pwd:%s]\n",transport->GetLocalPwd());
				//DOne
				return;
			}
			
			//Inc stats
			connection->iceRequestsReceived++;

			//Check if it has the prio attribute
			if (!stun->HasAttribute(STUNMessage::Attribute::Priority))
			{
				//Error
				Debug("-RTPBundleTransport::Read() | STUN Message without priority attribute\n");
				//DOne
				return;
			}
			
			//Check wether we have to reply to this message or not
			bool reply = !(connection->disableSTUNKeepAlive && transport->HasActiveRemoteCandidate());
			
			//Get attribute
			STUNMessage::Attribute* priority = stun->GetAttribute(STUNMessage::Attribute::Priority);
			
			//Get prio
			DWORD prio = priority ? get4(priority->attr,0) : 0;
			
			//Find candidate or try to create one if not present
			auto [itc, inserted] = candidates.try_emplace(remote,ip,port,transport);
			
			//Get candidate
			ICERemoteCandidate* candidate = &itc->second;
			
			//Check if it is not already present
			if (inserted)
			{
				Log("-RTPBundleTransport::Read() | Got new remote ICE candidate [remote:%s]\n",remote.c_str());
				//Add it to the connection
				connection->candidates.insert(candidate);
				//We need to reply the first always
				reply = true;
			}
			
			//Set it active
			transport->ActivateRemoteCandidate(candidate,stun->HasAttribute(STUNMessage::Attribute::UseCandidate),prio);
			
			//Create response
			auto resp = std::unique_ptr<STUNMessage>(stun->CreateResponse());
			
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(htonl(ip),htons(port));
			
			//Create new mesage
			Packet buffer;
		
			//Serialize and autenticate
			size_t len = resp->AuthenticatedFingerPrint(buffer.GetData(),buffer.GetCapacity(),transport->GetLocalPwd());
			
			//resize
			buffer.SetSize(len);

			//Send response
			loop.Send(ip,port,std::move(buffer));
			
			//Inc stats
			connection->iceResponsesSent++;

			//If the STUN keep alive response is not disabled
			if (reply)
				//Send back an ice request
				SendBindingRequest(connection,candidate);
		} else if (type==STUNMessage::Response && method==STUNMessage::Binding) {
			
			//Get ts and id
			uint32_t id = get4(stun->GetTransactionId(),0);
			uint64_t ts = get8(stun->GetTransactionId(),4);
			
			//Find transaction
			auto transactionIterator = transactions.find({ts,id});
			
			//If not found
			if (transactionIterator==transactions.end())
			{
				//Error
				Debug("-RTPBundleTransport::Read() | transaction not found [id:%u,ts:%llu]",id,ts);
				//Done
				return;
			}
			//Get username
			auto username = transactionIterator->second.first;
			
			//Delete transaction from list
			transactions.erase(transactionIterator);
				
			//Check if we have an ICE transport for that username
			auto cconnectionIterator = connections.find(username);
			
			//If not found
			if (cconnectionIterator==connections.end())
			{
				//Error
				Debug("-RTPBundleTransport::Read() | ICE username not found for response [%s]\n",username.c_str());
				//Done
				return;
			}
			
			//Get ice connection
			Connection* connection = cconnectionIterator->second;
			DTLSICETransport* transport = connection->transport;
			
			//Find candidate
			auto candidateIterator = candidates.find(remote);
			
			//Check we have it
			if (candidateIterator==candidates.end())
			{
				//Error
				Debug("-RTPBundleTransport::Read() | remote candidate not found for response [remote:%s]}\n",remote.c_str());
				return;
			}
		
			//Get it
			ICERemoteCandidate* candidate = &candidateIterator->second;
			
			//Authenticate request with remote username
			if (!stun->CheckAuthenticatedFingerPrint(data,size,transport->GetRemotePwd()))
			{
				//Error
				Error("-RTPBundleTransport::Read() | STUN Message response failed authentication [pwd:%s]\n",transport->GetRemotePwd());
				//DOne
				return;
			}

			//Get attribute
			STUNMessage::Attribute* priority = stun->GetAttribute(STUNMessage::Attribute::Priority);

			//Get prio
			DWORD prio = priority ? get4(priority->attr,0) : 0;

			//Set it active
			transport->ActivateRemoteCandidate(candidate,stun->HasAttribute(STUNMessage::Attribute::UseCandidate),prio);
			
			//Set state
			candidate->SetState(ICERemoteCandidate::Connected);
			
			//Inc stats
			connection->iceResponsesReceived++;
		}

		//Exit
		return;
	}
	
	//Find candidate
	auto it = candidates.find(remote);
	
	//Check if it was not registered
	if (it==candidates.end())
	{
		//Error
		Debug("-RTPBundleTransport::Read() | No registered ICE candidate for [%s]\n",remote.c_str());
		//DOne
		return;
	}
	
	//Send data on ice transport
	it->second.onData(data,size);
}

int RTPBundleTransport::AddRemoteCandidate(const std::string& username,const char* host, WORD port)
{
	Log("-RTPBundleTransport::AddRemoteCandidate() [username:%s,candidate:%s:%u}\n",username.c_str(),host,port);
	
	//Copy ip 
	auto ip = std::string(host);
	
	//Execute async and wait for completion
	loop.Async([=](...){
		//Check if we have an ICE transport for that username
		auto it = connections.find(username);

		//If not found
		if (it==connections.end())
		{
			//Exit
			Error("-RTPBundleTransport::AddRemoteCandidate() | ICE username not found [username:%s}\n",username.c_str());
			//Done
			return;
		}
		
		//Get ice connection
		Connection* connection = it->second;
		DTLSICETransport* transport = connection->transport;
		
		//Get remote ip:port address
		std::string remote = ip + ":" + std::to_string(port);
		
		//Create new candidate if it is not already present
		auto [itc, inserted] = candidates.try_emplace(remote,ip,port,transport);
		
		//Get candidate
		ICERemoteCandidate* candidate = &itc->second;
	
		//If it was new
		if (inserted)
			//Add candidate and add it to the connection
			connection->candidates.insert(candidate);

		//Send binding request in any case
		SendBindingRequest(connection,candidate);
		
	}).wait();
	
	return 1;
}


void RTPBundleTransport::SendBindingRequest(Connection* connection,ICERemoteCandidate* candidate)
{
	UltraDebug("-RTPBundleTransport::SendBindingRequest() [remote:%s]\n",candidate->GetRemoteAddress().c_str());
	
	//Get transport
	DTLSICETransport* transport = connection->transport;
	
	//Create transaction
	uint32_t id	= maxTransId++;
	uint64_t ts	= getTime();
	//Create trans id
	BYTE transId[12];
	//Set data
	set4(transId,0,id);
	set8(transId,4,ts);
	
	//Add to outgoing transactions
	transactions[{ts,id}] = {connection->username,candidate->GetRemoteAddress()};
				
	//Create binding request to send back
	auto request = std::make_unique<STUNMessage>(STUNMessage::Request,STUNMessage::Binding,transId);
	//Add username
	request->AddUsernameAttribute(transport->GetLocalUsername(),transport->GetRemoteUsername());

	//Add other attributes
	request->AddAttribute(STUNMessage::Attribute::IceControlled,(QWORD)1);
	request->AddAttribute(STUNMessage::Attribute::Priority,(DWORD)33554431);

	//Create new mesage
	Packet buffer;

	//Serialize and autenticate
	size_t len = request->AuthenticatedFingerPrint(buffer.GetData(),buffer.GetCapacity(),transport->GetRemotePwd());

	//resize
	buffer.SetSize(len);

	//Send it
	loop.Send(candidate->GetIPAddress(),candidate->GetPort(),std::move(buffer));
	
	//Set state
	candidate->SetState(ICERemoteCandidate::Checking);

	//Inc stats
	connection->iceRequestsSent++;
	
	//Check if we need to start timer
	if (iceTimer && !iceTimer->IsScheduled())
		//Set it again
		iceTimer->Again(iceTimeout);
}

void RTPBundleTransport::onTimer(std::chrono::milliseconds now)
{
	UltraDebug("-RTPBundleTransport::onTimer()\n");
	
	//Delete old transactions
	for (auto it = transactions.begin();it != transactions.end(); it = transactions.erase(it))
	{
		//Get transaction timestamp
		auto ts = std::chrono::milliseconds(it->first.first/1000);
		//Check if this is still valid
		if ( ts + iceTimeout > now)
		{
			//Fire the timer again for timing out the transaction
			iceTimer->Again(ts + iceTimeout - now);
			//Done
			return;
		}
		//Get username and remote address of ice candidate
		auto& [username,remote] = it->second;
		
		//Check if we still have an ICE transport for that username
		auto cconnectionIterator = connections.find(username);
			
		//If not found
		if (cconnectionIterator==connections.end())
			break;
			
		//Get ice connection
		Connection* connection = cconnectionIterator->second;
		
		//Find candidate
		auto candidateIterator = candidates.find(remote);
			
		//Check we have it
		if (candidateIterator==candidates.end())
			break;
		
		//Get it
		ICERemoteCandidate* candidate = &candidateIterator->second;
		
		//Check again
		SendBindingRequest(connection,candidate);
	}
}
