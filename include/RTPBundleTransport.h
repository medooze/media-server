#ifndef RTPBUNDLETRANSPORT_H
#define	RTPBUNDLETRANSPORT_H

#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <poll.h>
#include <srtp2/srtp.h>
#include "config.h"
#include "DTLSICETransport.h"
#include "EventLoop.h"

class RTPBundleTransport :
	public DTLSICETransport::Sender,
	public EventLoop::Listener
{
private:
	struct Connection
	{
		Connection(DTLSICETransport* transport,bool disableSTUNKeepAlive)
		{
			this->transport = transport;
			this->disableSTUNKeepAlive = disableSTUNKeepAlive;
		}
		
		DTLSICETransport* transport;
		std::set<ICERemoteCandidate*> candidates;
		bool disableSTUNKeepAlive;
	};
public:
	RTPBundleTransport();
	virtual ~RTPBundleTransport();
	int Init();
	int Init(int port);
	DTLSICETransport* AddICETransport(const std::string &username,const Properties& properties);
	int RemoveICETransport(const std::string &username);
	
	int End();
	
	int GetLocalPort() const { return port; }
	int AddRemoteCandidate(const std::string& username,const char* ip, WORD port);
	virtual int Send(const ICERemoteCandidate* candidate,Packet&& buffer) override;
	
	virtual void OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ip, const uint16_t port) override;
	
	bool SetAffinity(int cpu) { return loop.SetAffinity(cpu); }
private:
	typedef std::map<std::string,Connection*> Connections;
	typedef std::map<std::string,ICERemoteCandidate*> RTPICECandidates;
private:
	//Sockets
	int 	socket;
	int 	port;
	
	EventLoop loop;

	Connections	 connections;
	RTPICECandidates candidates;
	Use	use;
};

#endif
