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
#include <list>

#include "config.h"
#include "stunmessage.h"
#include "dtls.h"
#include "ICERemoteCandidate.h"
#include "rtp.h"
#include "fecdecoder.h"
#include "use.h"
#include "UDPDumper.h"
#include "remoterateestimator.h"
#include "EventLoop.h"
#include "Datachannels.h"
#include "Endpoint.h"
#include "SRTPSession.h"
#include "SendSideBandwidthEstimation.h"

class DTLSICETransport : 
	public RTPSender,
	public RTPReceiver,
	public DTLSConnection::Listener,
	public ICERemoteCandidate::Listener
{
public:
	enum DTLSState
	{
		New,
		Connecting,
		Connected,
		Closed,
		Failed
	};
	
	class Listener
	{
	public:
		virtual void onICETimeout() = 0;
		virtual void onDTLSStateChanged(const DTLSState) = 0;
		virtual void onRemoteICECandidateActivated(const std::string& ip, uint16_t port, uint32_t priority) = 0;
		virtual ~Listener() = default;
	};
	class Sender
	{
	public:
		virtual int Send(const ICERemoteCandidate *candiadte, Packet&& buffer) = 0;
	};

public:
	DTLSICETransport(Sender *sender,TimeService& timeService);
	virtual ~DTLSICETransport();
	
	void Start();
	void Stop();
	
	void SetSRTPProtectionProfiles(const std::string& profiles);
	void SetRemoteProperties(const Properties& properties);
	void SetLocalProperties(const Properties& properties);
	virtual int SendPLI(DWORD ssrc) override;
	virtual int Enqueue(const RTPPacket::shared& packet) override;
	virtual int Enqueue(const RTPPacket::shared& packet,std::function<RTPPacket::shared(const RTPPacket::shared&)> modifier) override;
	int Dump(const char* filename, bool inbound = true, bool outbound = true, bool rtcp = true, bool rtpHeadersOnly = false);
	int Dump(UDPDumper* dumper, bool inbound = true, bool outbound = true, bool rtcp = true, bool rtpHeadersOnly = false);
	int StopDump();
        int DumpBWEStats(const char* filename);
	int StopDumpBWEStats();
	void Reset();
	
	void ActivateRemoteCandidate(ICERemoteCandidate* candidate,bool useCandidate, DWORD priority);
	bool HasActiveRemoteCandidate() const { return active;	}
	int SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(const char* username, const char* pwd);
	bool AddOutgoingSourceGroup(RTPOutgoingSourceGroup *group);
	bool RemoveOutgoingSourceGroup(RTPOutgoingSourceGroup *group);
	bool AddIncomingSourceGroup(RTPIncomingSourceGroup *group);
	bool RemoveIncomingSourceGroup(RTPIncomingSourceGroup *group);
	
	void SetBandwidthProbing(bool probe);
	void SetMaxProbingBitrate(DWORD bitrate)	{ this->maxProbingBitrate = bitrate;	}
	void SetProbingBitrateLimit(DWORD bitrate)	{ this->probingBitrateLimit = bitrate;	}
	void SetSenderSideEstimatorListener(RemoteRateEstimator::Listener* listener) { senderSideBandwidthEstimator.SetListener(listener); }
	
	const char* GetRemoteUsername() const { return iceRemoteUsername;	};
	const char* GetRemotePwd()	const { return iceRemotePwd;		};
	const char* GetLocalUsername()	const { return iceLocalUsername;	};
	const char* GetLocalPwd()	const { return iceLocalPwd;		};
	
	virtual void onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize)  override;
	virtual void onDTLSPendingData() override;
	virtual void onDTLSSetupError() override;
	virtual void onDTLSShutdown() override;
	virtual int onData(const ICERemoteCandidate* candidate,const BYTE* data,DWORD size)  override;
	
	DWORD GetRTT() const { return rtt; }
	
	TimeService& GetTimeService() { return timeService; }
	
	void SetListener(Listener* listener);

private:
	void SetState(DTLSState state);
	void Probe();
	int Send(RTPPacket::shared&& packet);
	int Send(const RTCPCompoundPacket::shared& rtcp);
	void SetRTT(DWORD rtt);
	void onRTCP(const RTCPCompoundPacket::shared &rtcp);
	void ReSendPacket(RTPOutgoingSourceGroup *group,WORD seq);
	DWORD SendProbe(const RTPPacket::shared& packet);
	DWORD SendProbe(RTPOutgoingSourceGroup *group,BYTE padding);
	void SendTransportWideFeedbackMessage(DWORD ssrc);
	
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
	
private:
	Sender*		sender = nullptr;
	TimeService&	timeService;
	datachannels::impl::Endpoint endpoint;
	datachannels::Endpoint::Options dcOptions;
	Listener*	listener = nullptr;
	DTLSConnection	dtls;
	DTLSState	state = DTLSState::New;
	Maps		sendMaps;
	Maps		recvMaps;
	ICERemoteCandidate* active			= nullptr;
	SRTPSession	send;
	SRTPSession	recv;
	WORD		transportSeqNum			= 0;
	WORD		feedbackPacketCount		= 0;
	DWORD		lastFeedbackPacketExtSeqNum	= 0;
	WORD		feedbackCycles			= 0;
	OutgoingStreams outgoing;
	IncomingStreams incoming;
	std::map<std::string,RTPIncomingSourceGroup*> rids;
	std::map<std::string,std::set<RTPIncomingSourceGroup*>> mids;
	std::list<RTPPacket::shared> history;
	
	DWORD	mainSSRC		= 1;
	DWORD   rtt			= 0;
	char*	iceRemoteUsername	= nullptr;
	char*	iceRemotePwd		= nullptr;
	char*	iceLocalUsername	= nullptr;
	char*	iceLocalPwd		= nullptr;
	
	Acumulator incomingBitrate;
	Acumulator outgoingBitrate;
	Acumulator rtxBitrate;
	Acumulator probingBitrate;
	
	std::map<DWORD,PacketStats::shared> transportWideReceivedPacketsStats;
	
	UDPDumper* dumper		= nullptr;
	bool dumpInRTP			= false;
	bool dumpOutRTP			= false;
	bool dumpRTCP			= false;
	bool dumpRTPHeadersOnly		= false;
	volatile bool probe		= false;
	DWORD maxProbingBitrate		= 1024*1000;
	DWORD probingBitrateLimit	= maxProbingBitrate *4;
	
	Timer::shared probingTimer;
	QWORD   lastProbe = 0;
	QWORD 	initTime = 0;
	bool	started = false;
	
	SendSideBandwidthEstimation senderSideBandwidthEstimator;
	
	Timer::shared iceTimeoutTimer;
};


#endif	/* DTLSICETRANSPORT_H */

