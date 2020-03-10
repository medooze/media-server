/* 
 * File:   RTPTransport.h
 * Author: Sergio
 *
 * Created on 2 de noviembre de 2016, 12:20
 */

#ifndef RTPTRANSPORT_H
#define	RTPTRANSPORT_H

#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <poll.h>
#include <srtp2/srtp.h>
#include "config.h"
#include "stunmessage.h"
#include "dtls.h"
#include "EventLoop.h"
#include "Datachannels.h"
#include "Endpoint.h"
#include "PCAPFile.h"

class RTPTransport :
	public EventLoop::Listener,
	public DTLSConnection::Listener
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onRemotePeer(const char* ip, const short port) = 0;
		virtual void onRTPPacket(const BYTE* buffer, DWORD size) = 0;
		virtual void onRTCPPacket(const BYTE* buffer, DWORD size) = 0;
	};
public:

public:
	static bool SetPortRange(int minPort, int maxPort);
	static DWORD GetMinPort() { return minLocalPort; }
	static DWORD GetMaxPort() { return maxLocalPort; }

private:
	// Admissible port range
	static DWORD minLocalPort;
	static DWORD maxLocalPort;
	static int minLocalPortRange;

public:
	RTPTransport(Listener *listener);
	~RTPTransport();
	int Init();
	int SetLocalPort(int recvPort);
	int GetLocalPort();
	int SetRemotePort(char *ip,int sendPort);
	void Reset();
	int End();
	
	int SendRTPPacket(Packet&& packet);
	int SendRTCPPacket(Packet&& packet);

	int SetLocalCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(const char* username, const char* pwd);
	
	
	void SetMuxRTCP(int flag)	{ muxRTCP = flag; };
	void SetSecure(int flag)	{ encript = true; decript = true; };

	virtual void OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port) override;
	virtual void onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize) override;
	virtual void onDTLSPendingData() override;
	virtual void onDTLSSetupError() override;
	virtual void onDTLSShutdown() override;
	
	TimeService& GetTimeService() { return rtpLoop; }
	
private:
	void SendEmptyPacket();
	int SetLocalCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	int SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	void Start();
	void Stop();
	int  ReadRTP(const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port);
	int  ReadRTCP(const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port);

private:
	Listener* listener;
	bool	muxRTCP;
	//Sockets
	int 	simSocket;
	int 	simRtcpSocket;
	int 	simPort;
	int	simRtcpPort;
	EventLoop rtpLoop;
	EventLoop rtcpLoop;

	datachannels::impl::Endpoint endpoint;
	DTLSConnection dtls;
	bool	encript;
	bool	decript;
	srtp_t	send;
	srtp_t	recv;

	char*	iceRemoteUsername;
	char*	iceRemotePwd;
	char*	iceLocalUsername;
	char*	iceLocalPwd;

	//Transmision
	sockaddr_in sendAddr;
	sockaddr_in sendRtcpAddr;

	//Recepcion
	in_addr_t recIP;
	DWORD	  recPort;
	DWORD     prio;
	bool	  dumping;
	PCAPFile  pcap;
};

#endif
