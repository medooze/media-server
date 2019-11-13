#ifndef RTPBUNDLETRANSPORT_H
#define	RTPBUNDLETRANSPORT_H

#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <memory>
#include <poll.h>
#include <srtp2/srtp.h>
#include "config.h"
#include "DTLSICETransport.h"
#include "EventLoop.h"

class RTPBundleTransport :
	public DTLSICETransport::Sender,
	public EventLoop::Listener
{
public:
	struct Connection
	{
		Connection(const std::string& username, DTLSICETransport* transport,bool disableSTUNKeepAlive)
		{
			this->username = username;
			this->transport = transport;
			this->disableSTUNKeepAlive = disableSTUNKeepAlive;
		}
		
		std::string username;
		DTLSICETransport* transport;
		std::set<ICERemoteCandidate*> candidates;
		bool disableSTUNKeepAlive	= false;
		size_t iceRequestsSent		= 0;
		size_t iceRequestsReceived	= 0;
		size_t iceResponsesSent		= 0;
		size_t iceResponsesReceived	= 0;
		
	};
public:
	RTPBundleTransport();
	virtual ~RTPBundleTransport();
	int Init();
	int Init(int port);
	Connection* AddICETransport(const std::string &username,const Properties& properties);
	int RemoveICETransport(const std::string &username);
	
	int End();
	
	int GetLocalPort() const { return port; }
	int AddRemoteCandidate(const std::string& username,const char* ip, WORD port);
	virtual int Send(const ICERemoteCandidate* candidate,Packet&& buffer) override;
	
	virtual void OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ip, const uint16_t port) override;
	
	void SetIceTimeout(uint32_t timeout)	{ iceTimeout = std::chrono::milliseconds(timeout);	}
	bool SetAffinity(int cpu)		{ return loop.SetAffinity(cpu);				}
	TimeService& GetTimeService()		{ return loop;						}
private:
	void onTimer(std::chrono::milliseconds now);
	void SendBindingRequest(Connection* connection,ICERemoteCandidate* candidate);
private:
	//Sockets
	int 	socket;
	int 	port;
	
	EventLoop loop;
	Timer::shared iceTimer;
	std::chrono::milliseconds iceTimeout = 10000ms;

	std::map<std::string,Connection*>	 connections;
	std::map<std::string,ICERemoteCandidate> candidates;
	std::map<std::pair<uint64_t,uint32_t>,std::pair<std::string,std::string>> transactions;
	uint32_t maxTransId = 0;
	Use	use;
};

#endif
