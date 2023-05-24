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
#include "PacketHeader.h"

class RTPBundleTransport :
	public DTLSICETransport::Sender,
	public EventLoop::Listener
{
public:
	struct Connection
	{
		using shared = std::shared_ptr<Connection>;

		Connection(const std::string& username, DTLSICETransport::shared transport,bool disableSTUNKeepAlive)
		{
			this->username = username;
			this->transport = transport;
			this->disableSTUNKeepAlive = disableSTUNKeepAlive;
		}
		
		std::string username;
		DTLSICETransport::shared transport;
		std::set<ICERemoteCandidate*> candidates;
		bool disableSTUNKeepAlive	= false;
		size_t iceRequestsSent		= 0;
		size_t iceRequestsReceived	= 0;
		size_t iceResponsesSent		= 0;
		size_t iceResponsesReceived	= 0;
		uint64_t lastKeepAliveRequestSent	= 0;
		uint64_t lastKeepAliveRequestReceived	= 0;
		
	};
public:
	RTPBundleTransport(uint32_t packetPoolSize = 0);
	virtual ~RTPBundleTransport();
	int Init();
	int Init(int port);
	Connection::shared AddICETransport(const std::string &username,const Properties& properties);
	bool RestartICETransport(const std::string& username, const std::string& restarted, const Properties& properties);
	int RemoveICETransport(const std::string &username);
	
	int End();
	
	int GetLocalPort() const { return port; }
	int AddRemoteCandidate(const std::string& username,const char* ip, WORD port);
	void SetCandidateRawTxData(const std::string& ip, uint16_t port, uint32_t selfAddr, const std::string& dstLladdr);
	virtual int Send(const ICERemoteCandidate* candidate,Packet&& buffer, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback = std::nullopt) override;
	
	virtual void OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ip, const uint16_t port) override;
	
	void SetRawTx(int32_t ifindex, unsigned int sndbuf, bool skipQdisc, const std::string& selfLladdr, uint32_t fallbackSelfAddr, const std::string& fallbackDstLladdr, uint16_t port);
	void ClearRawTx();

	void SetIceTimeout(uint32_t timeout)	{ iceTimeout = std::chrono::milliseconds(timeout);	}
	bool SetAffinity(int cpu)		{ return loop.SetAffinity(cpu);				}
	bool SetThreadName(const std::string& name) { return loop.SetThreadName(name);			}
	bool SetPriority(int priority)		{ return loop.SetPriority(priority);			}
	TimeService& GetTimeService()		{ return loop;						}
private:
	void onTimer(std::chrono::milliseconds now);
	void SendBindingRequest(Connection::shared connection,ICERemoteCandidate* candidate);
private:
	//Sockets
	int 	socket;
	int 	port;
	
	EventLoop loop;
	Timer::shared iceTimer;
	std::chrono::milliseconds iceTimeout = 10000ms;

	std::map<std::string, Connection::shared>	connections;
	std::map<std::string, ICERemoteCandidate>	candidates;
	std::map<std::pair<uint64_t,uint32_t>, std::pair<std::string,std::string>> transactions;
	uint32_t maxTransId = 0;
	Use	use;
};

#endif
