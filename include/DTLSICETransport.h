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
#include <memory>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <list>
#include <srtp2/srtp.h>
#include <queue>

#include "config.h"
#include "stunmessage.h"
#include "dtls.h"
#include "ICERemoteCandidate.h"
#include "rtp.h"
#include "fecdecoder.h"
#include "use.h"
#include "rtpbuffer.h"
#include "PCAPFile.h"
#include "remoterateestimator.h"
#include "waitqueue.h"



class DTLSICETransport : 
	public RTPSender,
	public RTPReceiver,
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
	virtual ~DTLSICETransport();
	
	void Start();
	void Stop();
	
	void SetSRTPProtectionProfiles(const std::string& profiles);
	void SetRemoteProperties(const Properties& properties);
	void SetLocalProperties(const Properties& properties);
	virtual int SendPLI(DWORD ssrc) override;
	virtual int Enqueue(const RTPPacket::shared& packet) override;
	int Dump(const char* filename, bool inbound = true, bool outbound = true, bool rtcp = true);
	void Reset();
	
	void ActivateRemoteCandidate(ICERemoteCandidate* candidate,bool useCandidate, DWORD priority);
	int SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(const char* username, const char* pwd);
	bool AddOutgoingSourceGroup(RTPOutgoingSourceGroup *group);
	bool RemoveOutgoingSourceGroup(RTPOutgoingSourceGroup *group);
	bool AddIncomingSourceGroup(RTPIncomingSourceGroup *group);
	bool RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group);
	
	void SetSenderSideEstimatorListener(RemoteRateEstimator::Listener* listener) { senderSideEstimator.SetListener(listener); }
	
	const char* GetRemoteUsername() const { return iceRemoteUsername;	};
	const char* GetRemotePwd()	const { return iceRemotePwd;		};
	const char* GetLocalUsername()	const { return iceLocalUsername;	};
	const char* GetLocalPwd()	const { return iceLocalPwd;		};
	
	virtual void onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize)  override;
	virtual int onData(const ICERemoteCandidate* candidate,BYTE* data,DWORD size)  override;
	
	DWORD GetRTT() const { return rtt; }

private:
	int Run();
	int Send(const RTPPacket::shared& packet);
	void SetRTT(DWORD rtt);
	void onRTCP(const RTCPCompoundPacket::shared &rtcp);
	void ReSendPacket(RTPOutgoingSourceGroup *group,WORD seq);
	void SendTransportWideFeedbackMessage(DWORD ssrc);
	void Send(const RTCPCompoundPacket::shared& rtcp);
	int SetLocalCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	int SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	//Helpers
	RTPIncomingSourceGroup* GetIncomingSourceGroup(DWORD ssrc);
	RTPIncomingSource*	GetIncomingSource(DWORD ssrc);
	RTPOutgoingSourceGroup* GetOutgoingSourceGroup(DWORD ssrc);
	RTPOutgoingSource*	GetOutgoingSource(DWORD ssrc);

private:
	typedef std::map<DWORD,RTPOutgoingSourceGroup*> OutgoingStreams;
	typedef std::map<DWORD,RTPIncomingSourceGroup*> IncomingStreams;
	
	struct Maps
	{
		RTPMap		rtp;
		RTPMap		ext;
		RTPMap		apt;
	};
	
private:
	struct PacketStats
	{
		using shared = std::shared_ptr<PacketStats>;
		
		static PacketStats::shared Create(const RTPPacket::shared& packet, DWORD size, QWORD now)
		{
			auto stats = std::make_shared<PacketStats>();
			
			stats->transportWideSeqNum	= packet->GetTransportSeqNum();
			stats->ssrc			= packet->GetSSRC();
			stats->extSeqNum		= packet->GetExtSeqNum();
			stats->size			= size;
			stats->payload			= packet->GetMediaLength();
			stats->timestamp		= packet->GetTimestamp();
			stats->time			= now;
			
			return stats;
		}
		DWORD transportWideSeqNum;
		DWORD ssrc;
		DWORD extSeqNum;
		DWORD size;
		DWORD payload;
		DWORD timestamp;
		QWORD time;
	};
private:
	Sender*		sender;
	DTLSConnection	dtls;
	Maps		sendMaps;
	Maps		recvMaps;
	ICERemoteCandidate* active			= nullptr;
	srtp_t		send				= nullptr;
	srtp_t		recv				= nullptr;
	WORD		transportSeqNum			= 0;
	WORD		feedbackPacketCount		= 0;
	DWORD		lastFeedbackPacketExtSeqNum	= 0;
	WORD		feedbackCycles			= 0;
	OutgoingStreams outgoing;
	IncomingStreams incoming;
	std::map<std::string,RTPIncomingSourceGroup*> rids;
	
	WaitCondition wait;
	std::queue<RTPPacket::shared> packets;
	pthread_t thread	= {0};
	bool running		= false;
	
	DWORD	mainSSRC		= 1;
	DWORD   rtt			= 0;
	char*	iceRemoteUsername	= nullptr;
	char*	iceRemotePwd		= nullptr;
	char*	iceLocalUsername	= nullptr;
	char*	iceLocalPwd		= nullptr;
	
	Mutex	mutex;
	Use	incomingUse;
	Use	outgoingUse;
	
	std::map<DWORD,PacketStats::shared> transportWideSentPacketsStats;
	std::map<DWORD,PacketStats::shared> transportWideReceivedPacketsStats;
	
	PCAPFile* pcap	= nullptr;
	bool dumpInRTP	= false;
	bool dumpOutRTP = false;
	bool dumpRTCP	= false;
	
	RemoteRateEstimator senderSideEstimator;
};


#endif	/* DTLSICETRANSPORT_H */

