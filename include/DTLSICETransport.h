/* 
 * File:   RTPICETransport.h
 * Author: Sergio
 *
 * Created on 8 de enero de 2017, 18:37
 */

#ifndef DTLSICETRANSPORT_H
#define	DTLSICETRANSPORT_H

#include <string>
#include <map>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <list>
#include <srtp2/srtp.h>
#include "config.h"
#include "stunmessage.h"
#include "dtls.h"
#include "ICERemoteCandidate.h"
#include "rtp.h"
#include "fecdecoder.h"
#include "use.h"
#include "rtpbuffer.h"

class DTLSICETransport : 
	public DTLSConnection::Listener,
	public ICERemoteCandidate::Listener
{
public:
	class Sender
	{
	public:
		virtual int Send(const ICERemoteCandidate *candiadte, const BYTE* data,const DWORD size) = 0;
	};

public:
	DTLSICETransport(Sender *sender);
	~DTLSICETransport();
	void SetRemoteProperties(const Properties& properties);
	void SetLocalProperties(const Properties& properties);
	void SendPLI(DWORD ssrc);
	void Send(RTPPacket &packet);
	void Reset();
	
	ICERemoteCandidate* AddRemoteCandidate(const sockaddr_in addr, bool useCandidate, DWORD priority);
	int SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(const char* username, const char* pwd);
	bool AddOutgoingSourceGroup(RTPOutgoingSourceGroup *group);
	bool RemoveOutgoingSourceGroup(RTPOutgoingSourceGroup *group);
	bool AddIncomingSourceGroup(RTPIncomingSourceGroup *group);
	bool RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group);
	
	
	const char* GetRemoteUsername() const { return iceRemoteUsername;	};
	const char* GetRemotePwd()	const { return iceRemotePwd;		};
	const char* GetLocalUsername()	const { return iceLocalUsername;	};
	const char* GetLocalPwd()	const { return iceLocalPwd;		};
	
	virtual void onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize);
	virtual int onData(const ICERemoteCandidate* candidate,BYTE* data,DWORD size);
private:
	void onRTCP(RTCPCompoundPacket* rtcp);
	void ReSendPacket(RTPOutgoingSourceGroup *group,int seq);
	void Send(RTCPCompoundPacket &rtcp);
	int SetLocalCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	int SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
private:
	typedef std::list<ICERemoteCandidate*> Candidates;
	typedef std::map<DWORD,RTPOutgoingSourceGroup*> OutgoingStreams;
	typedef std::map<DWORD,RTPIncomingSourceGroup*> IncomingStreams;
	
	struct Maps
	{
		RTPMap		rtp;
		RTPMap		ext;
		RTPMap		apt;
	};
private:
	Sender*		sender;
	DTLSConnection	dtls;
	Maps		sendMaps;
	Maps		recvMaps;
	Candidates	candidates;
	ICERemoteCandidate* active;
	srtp_t		send;
	srtp_t		recv;
	WORD		transportSeqNum;
	WORD		feedbackPacketCount;
	WORD		lastFeedbackPacketExtSeqNum;
	WORD		feedbackCycles;
	OutgoingStreams outgoing;
	IncomingStreams incoming;
	
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
	
	Mutex	mutex;
};


#endif	/* DTLSICETRANSPORT_H */

