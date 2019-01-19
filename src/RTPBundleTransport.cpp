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
	loop(*this)
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
	auto connection = new Connection(transport,properties.GetProperty("disableSTUNKeepAlive", false));
	
	//Synchronized
	loop.Async([=](...){
		//Add it
		connections[username] = connection;
		//Start it
		transport->Start();
	});
	
	//OK
	return transport;
}

int RTPBundleTransport::RemoveICETransport(const std::string &username)
{
	Log("-RTPBundleTransport::RemoveICETransport() [username:%s]\n",username.c_str());
  
	//Synchronized
	loop.Async([this,username](...){

		//Get transport
		auto it = connections.find(username);

		//Check
		if (it==connections.end())
		{
			//Error
			Error("-ICE transport not found\n");
			//Done
			return;
		}

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
		loop.Start(socket);
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
	loop.Start(socket);
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
	Log(">RTPBundleTransport::End()\n");

	//Stop just in case
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

int RTPBundleTransport::Send(const ICERemoteCandidate* candidate, Buffer&& buffer)
{
	loop.Send(candidate->GetIPAddress(),candidate->GetPort(),std::move(buffer));
	return 1;
}

void RTPBundleTransport::OnRead(const uint8_t* data, const size_t size, const uint32_t ip, const uint16_t port)
{
	//Get remote ip:port address
	char remote[24];
	const uint8_t* host = (const uint8_t*)&ip;
	snprintf(remote,sizeof(remote),"%hu.%hu.%hu.%hu:%hu",get1(host,3), get1(host,2), get1(host,1), get1(host,0), port);
	
	//Check if it looks like a STUN message
	if (STUNMessage::IsSTUN(data,size))
	{
		//Parse it
		STUNMessage *stun = STUNMessage::Parse(data,size);

		//It was not a valid STUN message
		if (!stun)
		{
			//Error
			Error("-RTPBundleTransport::ReadRTP() | failed to parse STUN message\n");
			//Done
			return;
		}

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
			{
				//TODO: Reject
				//Error
				Error("-RTPBundleTransport::Read ice username not found [%s}\n",username.c_str());
				//Done
				return;
			}
			
			//Get ice connection
			Connection* connection = it->second;
			DTLSICETransport* transport = connection->transport;
			ICERemoteCandidate* candidate = NULL;

			//Check if it has the prio attribute
			if (!stun->HasAttribute(STUNMessage::Attribute::Priority))
			{
				//Error
				Error("-STUN Message without priority attribute");
				//DOne
				return;
			}
			
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
				candidate = new ICERemoteCandidate(ip,port,transport);
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
			resp->AddXorAddressAttribute(htonl(ip),htons(port));
			
			//Create new mesage
			Buffer buffer(MTU);
		
			//Serialize and autenticate
			size_t len = resp->AuthenticatedFingerPrint(buffer.GetData(),buffer.GetCapacity(),transport->GetLocalPwd());
			
			//resize
			buffer.SetSize(len);

			//Send response
			loop.Send(ip,port,std::move(buffer));

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

				//Create new mesage
				Buffer buffer(MTU);
				
				//Serialize and autenticate
				size_t len = request->AuthenticatedFingerPrint(buffer.GetData(),buffer.GetCapacity(),transport->GetRemotePwd());

				//resize
				buffer.SetSize(len);
				
				//Send response
				loop.Send(ip,port,std::move(buffer));
				
				//Clean response
				delete(request);
			}
		}

		//Delete message
		delete(stun);

		//Exit
		return;
	}
	
	//Find candidate
	auto it = candidates.find(remote);
	
	//Check if it was not registered
	if (it==candidates.end())
	{
		//Error
		Error("-RTPBundleTransport::ReadRTP() | No registered ICE candidate for [%s]\n",remote);
		//DOne
		return;
	}
	
	//Get ice transport
	ICERemoteCandidate *ice = it->second;
	
	//Send data
	ice->onData(data,size);
}

int RTPBundleTransport::AddRemoteCandidate(const std::string& username,const char* ip, WORD port)
{
	//Get remote ip:port address
	std::string remote = std::string(ip) + ":" + std::to_string(port);
		
	//Async
	loop.Async([=](...){
		//Check if we have an ICE transport for that username
		auto it = connections.find(username);

		//If not found
		if (it==connections.end())
		{
			//Exit
			Error("-RTPBundleTransport::AddRemoteCandidate:: ice username not found [%s}\n",username.c_str());
			//Done
			return;
		}

		//Get ice connection
		Connection* connection = it->second;
		DTLSICETransport* transport = connection->transport;

		//Check if it is not already present
		if (candidates.find(remote)!=candidates.end())
		{
			//Exit
			Error("-RTPBundleTransport::AddRemoteCandidate already present [candidate:%s]\n",remote);
			//Done
			return;
		}

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

		//Create new mesage
		Buffer buffer(MTU);
		
		//Serialize and autenticate
		size_t len = request->AuthenticatedFingerPrint(buffer.GetData(),buffer.GetCapacity(),transport->GetRemotePwd());
		
		//resize
		buffer.SetSize(len);

		//Send it
		loop.Send(candidate->GetIPAddress(),candidate->GetPort(),std::move(buffer));

		//Clean request
		delete(request);
	});
	
	return 1;
}
