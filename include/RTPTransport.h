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

class RTPTransport :
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
		virtual void onRTPPacket(BYTE* buffer, DWORD size) = 0;
		virtual void onRTCPPacket(BYTE* buffer, DWORD size) = 0;
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

	int SendRTPPacket(BYTE *buffer,DWORD size);
	int SendRTCPPacket(BYTE *buffer,DWORD size);

	int SetLocalCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(const char* username, const char* pwd);
	
	
	void SetMuxRTCP(int flag)	{ muxRTCP = flag; };
	void SetSecure(int flag)	{ encript = decript = true; };

	virtual void onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize);
private:
	void SendEmptyPacket();
	int SetLocalCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	int SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	void Start();
	void Stop();
	int  ReadRTP();
	int  ReadRTCP();
	int Run();
private:
	static  void* run(void *par);
private:
	Listener* listener;
	bool	muxRTCP;
	//Sockets
	int 	simSocket;
	int 	simRtcpSocket;
	int 	simPort;
	int	simRtcpPort;
	pollfd	ufds[2];
	bool	inited;
	bool	running;

	DTLSConnection dtls;
	bool	encript;
	bool	decript;
	srtp_t	sendSRTPSession;
	srtp_t	recvSRTPSession;

	char*	iceRemoteUsername;
	char*	iceRemotePwd;
	char*	iceLocalUsername;
	char*	iceLocalPwd;
	pthread_t thread;

	//Transmision
	sockaddr_in sendAddr;
	sockaddr_in sendRtcpAddr;

	//Recepcion
	in_addr_t recIP;
	DWORD	  recPort;
	DWORD     prio;
};

#endif
