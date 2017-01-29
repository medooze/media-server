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

struct RTPIncomingFecSource : public RTPIncomingSource
{
	FECDecoder decoder;
};


class RTPOutgoingSourceGroup
{
public:
	class Listener 
	{
		public:
			virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) = 0;
		
	};
public:
	RTPOutgoingSourceGroup(std::string &streamId,MediaFrame::Type type,Listener* listener)
	{
		this->streamId = streamId;
		this->type = type;
		this->listener = listener;
	};
public:
	typedef std::map<DWORD,RTPTimedPacket*> RTPOrderedPackets;
public:	
	std::string streamId;
	MediaFrame::Type type;
	RTPOutgoingSource media;
	RTPOutgoingSource fec;
	RTPOutgoingRtxSource rtx;
	RTPOrderedPackets	packets;
	Listener* listener;
};

struct RTPIncomingSourceGroup
{
public:
	class Listener 
	{
		public:
			virtual void onRTP(RTPIncomingSourceGroup* group,RTPTimedPacket* packet) = 0;
	};
public:	
	RTPIncomingSourceGroup(MediaFrame::Type type,Listener* listener) : losts(64)
	{
		this->type = type;
		this->listener = listener;
	};
	int onRTP(DWORD ssrc,BYTE codec,BYTE* data,DWORD size);
public:	
	MediaFrame::Type type;
	RTPLostPackets	losts;
	RTPIncomingSource media;
	RTPIncomingFecSource fec;
	RTPIncomingRtxSource rtx;
	Listener* listener;
};


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
	int SetRemotePort(char *ip,int sendPort);
	void SetProperties(const Properties& properties);
	void SendPLI(DWORD ssrc);
	void Send(RTPTimedPacket &packet);
	void Reset();
	int End();
	
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
private:
	Sender*		sender;
	DTLSConnection	dtls;
	RTPMap		rtpMap;
	RTPMap		extMap;
	Candidates	candidates;
	ICERemoteCandidate* active;
	srtp_t		sendSRTPSession;
	srtp_t		recvSRTPSession;
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
};


#endif	/* DTLSICETRANSPORT_H */

