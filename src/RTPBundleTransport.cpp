#include "tracing.h"
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
#include <memory>
#include <optional>
#include <stdexcept>
#include <system_error>
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
#include "MacAddress.h"

#ifndef __linux__
void RTPBundleTransport::SetRawTx(int32_t ifindex, unsigned int sndbuf, bool skipQdisc, const std::string& selfLladdr, uint32_t defaultSelfAddr, const std::string& defaultDstLladdr, uint16_t port)
{
	throw std::runtime_error("raw TX is only supported in Linux");
}
#else

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <fcntl.h>

void RTPBundleTransport::SetRawTx(int32_t ifindex, unsigned int sndbuf, bool skipQdisc, const std::string& selfLladdr, uint32_t defaultSelfAddr, const std::string& defaultDstLladdr, uint16_t port)
{

	// prepare frame template

	PacketHeader header = PacketHeader::Create(MacAddress::Parse(selfLladdr), port);

	PacketHeader::FlowRoutingInfo defaultRoute = { defaultSelfAddr, MacAddress::Parse(defaultDstLladdr) };

	// set up AF_PACKET socket
	// protocol=0 means no RX
	FileDescriptor fd(::socket(PF_PACKET, SOCK_RAW, 0));

	if (!fd.isValid())
		throw std::system_error(std::error_code(errno, std::system_category()), "failed creating AF_PACKET socket");

	(void)fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

	struct sockaddr_ll bindAddr;
	bindAddr.sll_family = AF_PACKET;
	bindAddr.sll_ifindex = ifindex;
	bindAddr.sll_protocol = 0;
	if (bind(fd, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed binding AF_PACKET socket");

	if (sndbuf > 0 && setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed setting send queue size");

	int skipQdiscInt = 1;
	if (skipQdisc && setsockopt(fd, SOL_PACKET, PACKET_QDISC_BYPASS, &skipQdiscInt, sizeof(skipQdiscInt)) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed setting QDISC_BYPASS");

	loop.Async([=, fd = std::move(fd)](std::chrono::milliseconds) {
		loop.SetRawTx(fd, header, defaultRoute);
	});
}
#endif

void RTPBundleTransport::RTPBundleTransport::ClearRawTx()
{
	loop.Async([=](std::chrono::milliseconds) { 
		loop.ClearRawTx(); 
	}); 
}

/*************************
* RTPBundleTransport
* 	Constructro
**************************/
RTPBundleTransport::RTPBundleTransport(uint32_t packetPoolSize) :
	loop(this, packetPoolSize)
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

RTPBundleTransport::Connection::shared RTPBundleTransport::AddICETransport(const std::string &username,const Properties& properties)
{
	TRACE_EVENT("transport", "RTPBundleTransport::AddICETransport", "username", username);
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
	auto transport = std::make_shared<DTLSICETransport>(this,loop,loop.GetPacketPool());
	
	//Set SRTP protection profiles
	std::string profiles = properties.GetProperty("srtpProtectionProfiles","");
	transport->SetSRTPProtectionProfiles(profiles);
	
	//Set local STUN properties
	transport->SetLocalSTUNCredentials(ice.GetProperty("localUsername"),ice.GetProperty("localPassword"));
	transport->SetRemoteSTUNCredentials(ice.GetProperty("remoteUsername"),ice.GetProperty("remotePassword"));

	//Check if remb is disabled
	bool disableREMB = properties.GetProperty("remb.disabled", false);
	//If we need to disable it
	if (disableREMB) transport->DisableREMB(disableREMB);
	
	
	//Set remote DTLS 
	transport->SetRemoteCryptoDTLS(dtls.GetProperty("setup"),dtls.GetProperty("hash"),dtls.GetProperty("fingerprint"));
	
	//Create connection
	auto connection = std::make_shared<Connection>(username,transport,properties.GetProperty("disableSTUNKeepAlive", false));
	
	//Synchronized
	loop.Async([=](auto now){
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
	TRACE_EVENT("transport", "RTPBundleTransport::RemoveICETransport", "username", username);
	Log("-RTPBundleTransport::RemoveICETransport() [username:%s]\n",username.c_str());
  
	//Synchronized
	loop.Async([=](auto now){

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
		auto connection = connectionIterator->second;

		//Stop transport first, to prevent using active candidate after it is deleted afterwards
		connection->transport->Stop();

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
	});

	//DOne
	return 1;
}

bool RTPBundleTransport::RestartICETransport(const std::string& username, const std::string& restarted, const Properties& properties)
{
	TRACE_EVENT("transport", "RTPBundleTransport::RestartICETransport", "username", username);
	Log("-RTPBundleTransport::RestartICETransport() [username:%s,restarted:%s]\n", username.c_str(), restarted.c_str());

	Properties ice;
	
	//Get child properties
	properties.GetChildren("ice", ice);


	//Ensure that we have required ICE properties
	if (!ice.HasProperty("remoteUsername") || !ice.HasProperty("remotePassword") || !ice.HasProperty("localUsername") || !ice.HasProperty("localPassword"))
	{
		//Error
		Error("-RTPBundleTransport::RestartICETransport() | Missing ICE properties\n");
		//Error
		return false;
	}

	//Synchronized
	loop.Async([=](auto now) {

		//Get transport
		auto connectionIterator = connections.find(username);

		//Check
		if (connectionIterator == connections.end())
		{
			//Error
			Error("-RTPBundleTransport::RestartICETransport() | ICE transport not found\n");
			//Done
			return;
		}

		//Get connection 
		auto connection = connectionIterator->second;

		//REmove connection
		connections.erase(connectionIterator);

		//Set local STUN properties
		connection->transport->SetLocalSTUNCredentials(ice.GetProperty("localUsername"), ice.GetProperty("localPassword"));
		connection->transport->SetRemoteSTUNCredentials(ice.GetProperty("remoteUsername"), ice.GetProperty("remotePassword"));

		//Add it with new username
		connection->username = restarted;
		connections[restarted] = connection;
	});

	//DOne
	return 1;
}

int RTPBundleTransport::Init()
{
	int retries = 0;

	TRACE_EVENT("transport", "RTPBundleTransport::Init");
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
		//Try to bind to port
		recAddr.sin_port = htons(port);
		//Bind the rtp socket
// Ignore coverity error: "this->socket" is passed to a parameter that cannot be negative.
// coverity[negative_returns]
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
		(void)setsockopt(socket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
#endif
		//Set TOS
		int tos = 0x2E;
		(void)setsockopt(socket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
		
#ifdef IP_PMTUDISC_DONT			
		//Disable path mtu discoveruy
		int pmtu = IP_PMTUDISC_DONT;
		(void)setsockopt(socket, IPPROTO_IP, IP_MTU_DISCOVER, &pmtu, sizeof(pmtu));
#endif
		
		//Everything ok
		Log("-RTPBundleTransport::Init() | Got port [%d]\n",port);
		//Start receiving
		loop.Start(socket);
		//Create ice timer
		iceTimer = loop.CreateTimer([=](std::chrono::milliseconds now){ this->onTimer(now); });
		//Set name for debug
		iceTimer->SetName("RTPBundleTransport - ice");
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
	
	TRACE_EVENT("transport", "RTPBundleTransport::Init", "port", port);
	Log(">RTPBundleTransport::Init(%d)\n",port);

	sockaddr_in recAddr;

	//Clear addr
	memset(&recAddr,0,sizeof(struct sockaddr_in));

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
// Ignore coverity error: "this->socket" is passed to a parameter that cannot be negative.
// coverity[negative_returns]
	if(bind(socket,(struct sockaddr *)&recAddr,sizeof(struct sockaddr_in))!=0)
		//Error
		return Error("-RTPBundleTransport::Init() | could not open port\n");
	
#ifdef SO_PRIORITY
	//Set COS
	int cos = 5;
	(void)setsockopt(socket, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos));
#endif
	//Set TOS
	int tos = 0x2E;
	(void)setsockopt(socket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
	
#ifdef IP_PMTUDISC_DONT	
	//Disable path mtu discoveruy
	int pmtu = IP_PMTUDISC_DONT;
	(void)setsockopt(socket, IPPROTO_IP, IP_MTU_DISCOVER, &pmtu, sizeof(pmtu));
#endif
	//Everything ok
	Log("-RTPBundleTransport::Init() | Got port [%d]\n",port);
	//Store local port
	this->port = port;
	//Start receiving
	loop.Start(socket);
	
	//Create ice timer
	iceTimer = loop.CreateTimer([=](std::chrono::milliseconds now){ this->onTimer(now); });
	//Set name for debug
	iceTimer->SetName("RTPBundleTransport - ice");

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
	
	TRACE_EVENT("transport", "RTPBundleTransport::End");
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

int RTPBundleTransport::Send(const ICERemoteCandidate* candidate, Packet&& buffer, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback)
{
	loop.Send(candidate->GetIPAddress(),candidate->GetPort(),std::move(buffer),candidate->GetRawTxData(), callback);
	return 1;
}

void RTPBundleTransport::OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ip, const uint16_t port)
{
	TRACE_EVENT("transport", "RTPBundleTransport::OnRead", "ip", ip, "port", port, "size", size);

	//Get remote ip:port address
	std::string remote = ICERemoteCandidate::GetRemoteAddress(ip,port);
	
	//UltraDebug("-RTPBundleTransport::OnRead() | [remote:%s,size:%u]\n",remote.c_str(),size);
			
	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(data,size))
	{
		TRACE_EVENT("transport", "RTPBundleTransport::OnRead::STUN", "ip", ip, "port", port, "size", size);

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
			auto connection = it->second;
			auto transport = connection->transport;
			
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
				//Send back an ice request
				SendBindingRequest(connection, candidate);
			}
			
			//Set it active
			transport->ActivateRemoteCandidate(candidate,stun->HasAttribute(STUNMessage::Attribute::UseCandidate),prio);
			
			//Create response
			auto resp = std::unique_ptr<STUNMessage>(stun->CreateResponse());
			
			//Add received xor mapped addres
			resp->AddXorAddressAttribute(htonl(ip),htons(port));
			
			//Create new mesage
			Packet buffer = loop.GetPacketPool().pick();
		
			//Serialize and autenticate
			size_t len = resp->AuthenticatedFingerPrint(buffer.GetData(),buffer.GetCapacity(),transport->GetLocalPwd());
			
			//resize
			buffer.SetSize(len);

			//Send response
			Send(candidate, std::move(buffer));
			
			//Inc stats
			connection->iceResponsesSent++;

		} else if (type==STUNMessage::Response && method==STUNMessage::Binding) {
			
			//Get ts and id
			uint32_t id = get4(stun->GetTransactionId(),0);
			uint64_t ts = get8(stun->GetTransactionId(),4);

			//UltraDebug("-RTPBundleTransport::OnRead() | Binding response [id:%u,ts:%llu]\n", id, ts);
			
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
			auto connection = cconnectionIterator->second;
			auto transport = connection->transport;
			
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
			connection->lastKeepAliveRequestReceived = getTime();
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

void RTPBundleTransport::SetCandidateRawTxData(const std::string& ip, uint16_t port, uint32_t selfAddr, const std::string& dstLladdr)
{
	PacketHeader::FlowRoutingInfo rawTxData = { selfAddr, MacAddress::Parse(dstLladdr) };
	loop.Async([=](auto now){
		std::string remote = ip + ":" + std::to_string(port);

		auto it = candidates.find(remote);
		if (it == candidates.end())
		{
			Error("-RTPBundleTransport::SetCandidateRawTxData() | candidate not found [remote:%s}\n", remote.c_str());
			return;
		}

		printf("setting candidate %s data\n", remote.c_str());
		it->second.SetRawTxData(rawTxData);
	});
}

int RTPBundleTransport::AddRemoteCandidate(const std::string& username,const char* host, WORD port)
{
	TRACE_EVENT("transport", "RTPBundleTransport::AddRemoteCandidate", "username", username, "host", host, "port", port);
	Log("-RTPBundleTransport::AddRemoteCandidate() [username:%s,candidate:%s:%u}\n",username.c_str(),host,port);
	
	//Copy ip 
	auto ip = std::string(host);
	
	//Execute Sync
	loop.Async([=](auto now){
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
		auto connection = it->second;
		auto transport = connection->transport;
		
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
		
	});
	
	return 1;
}


void RTPBundleTransport::SendBindingRequest(Connection::shared connection,ICERemoteCandidate* candidate)
{
	TRACE_EVENT("transport", "RTPBundleTransport::SendBindingRequest");

	//Double check
	if (!connection || !candidate)
	{
		//Exit
		Error("-RTPBundleTransport::SendBindingRequest() | Null connection or candidate, skipping\n");
		//Done
		return;
	}

	//Get transport
	auto transport = connection->transport;
	
	//Create transaction
	uint32_t id	= maxTransId++;
	uint64_t ts	= getTime();

	//UltraDebug("-RTPBundleTransport::SendBindingRequest() [remote:%s,id:%u,ts:%llu]\n", candidate->GetRemoteAddress().c_str(), id, ts);

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
	Packet buffer = loop.GetPacketPool().pick();

	//Serialize and autenticate
	size_t len = request->AuthenticatedFingerPrint(buffer.GetData(),buffer.GetCapacity(),transport->GetRemotePwd());

	//resize
	buffer.SetSize(len);

	//Send it
	Send(candidate, std::move(buffer));
	
	//Set state
	candidate->SetState(ICERemoteCandidate::Checking);

	//Inc stats
	connection->iceRequestsSent++;

	//Check if it is the active candidate
	if (connection->transport->GetActiveRemoteCandidate() == candidate)
		//Onlyt consider timeouts for the active candidates
		connection->lastKeepAliveRequestSent = ts;
	
	//Check if we need to start timer
	if (iceTimer && !iceTimer->IsScheduled())
		//Set it again
		iceTimer->Again(iceTimeout);
}

void RTPBundleTransport::onTimer(std::chrono::milliseconds now)
{
	TRACE_EVENT("transport", "RTPBundleTransport::onTimer");
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
			break;
		}
		//Get username and remote address of ice candidate
		auto& [username,remote] = it->second;
		
		//Check if we still have an ICE transport for that username
		auto cconnectionIterator = connections.find(username);
			
		//If not found
		if (cconnectionIterator==connections.end())
			continue;
			
		//Get ice connection
		auto connection = cconnectionIterator->second;
		
		//Find candidate
		auto candidateIterator = candidates.find(remote);
			
		//Check we have it
		if (candidateIterator==candidates.end())
			continue;
		
		//Get it
		ICERemoteCandidate* candidate = &candidateIterator->second;
		
		//Check again
		SendBindingRequest(connection,candidate);
	}


	//Keepalive all the connections 
	for (auto& [username, connection] : connections)
	{
		//If it is disabled
		if (connection->disableSTUNKeepAlive)
			//Skip
			continue;

		//If there is an outgoing transaction already
		if (connection->lastKeepAliveRequestSent>connection->lastKeepAliveRequestReceived)
			//Skip
			continue;
		//Get active candidate
		auto active = connection->transport->GetActiveRemoteCandidate();

		//If we have an active remote candidate
		if (active)
			//Keep alive
			SendBindingRequest(connection, active);
	}
}
