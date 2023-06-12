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
	using shared = std::shared_ptr<DTLSICETransport>;
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
		using shared = std::shared_ptr<Listener>;
	public:
		virtual void onICETimeout() = 0;
		virtual void onDTLSStateChanged(const DTLSState) = 0;
		virtual void onRemoteICECandidateActivated(const std::string& ip, uint16_t port, uint32_t priority) = 0;
		virtual ~Listener() = default;
	};
	class Sender
	{
	public:
		virtual int Send(const ICERemoteCandidate *candiadte, Packet&& buffer, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback = std::nullopt) = 0;
	};

public:
	DTLSICETransport(Sender *sender,TimeService& timeService, ObjectPool<Packet>& packetPool);
	virtual ~DTLSICETransport();
	
	void Start();
	void Stop();
	
	void SetSRTPProtectionProfiles(const std::string& profiles);
	void SetRemoteProperties(const Properties& properties);
	void SetLocalProperties(const Properties& properties);
	virtual int SendPLI(DWORD ssrc) override;
	virtual int Reset(DWORD ssrc) override;
	virtual int Enqueue(const RTPPacket::shared& packet) override;
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
	bool AddOutgoingSourceGroup(const RTPOutgoingSourceGroup::shared& group);
	bool RemoveOutgoingSourceGroup(const RTPOutgoingSourceGroup::shared& group);
	bool AddIncomingSourceGroup(const RTPIncomingSourceGroup::shared& group);
	bool RemoveIncomingSourceGroup(const RTPIncomingSourceGroup::shared& group);
	
	void SetBandwidthProbing(bool probe);
	void SetMaxProbingBitrate(DWORD bitrate);
	void SetProbingBitrateLimit(DWORD bitrate);
	void EnableSenderSideEstimation(bool enabled);
	void SetSenderSideEstimatorListener(RemoteRateEstimator::Listener* listener) { senderSideBandwidthEstimator->SetListener(listener); }

	uint32_t GetAvailableOutgoingBitrate() const	{ return senderSideBandwidthEstimator->GetAvailableBitrate(); }
	uint32_t GetEstimatedOutgoingBitrate() const    { return senderSideBandwidthEstimator->GetEstimatedBitrate(); }
	uint32_t GetTotalSentBitrate() const		{ return senderSideBandwidthEstimator->GetTotalSentBitrate(); }

	void SetRemoteOverrideBWE(bool overrideBew);
	void SetRemoteOverrideBitrate(DWORD bitrate);
	void DisableREMB(bool disabled);
	
	ICERemoteCandidate* GetActiveRemoteCandidate() const { return active;	};
	const char* GetRemoteUsername() const { return iceRemoteUsername.c_str();	};
	const char* GetRemotePwd()	const { return iceRemotePwd.c_str();		};
	const char* GetLocalUsername()	const { return iceLocalUsername.c_str();	};
	const char* GetLocalPwd()	const { return iceLocalPwd.c_str();		};
	
	virtual void onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize)  override;
	virtual void onDTLSPendingData() override;
	virtual void onDTLSSetupError() override;
	virtual void onDTLSShutdown() override;
	virtual int onData(const ICERemoteCandidate* candidate,const BYTE* data,DWORD size)  override;
	
	DWORD GetRTT() const { return rtt; }
	
	TimeService& GetTimeService() { return timeService; }
	
	void SetListener(const Listener::shared& listener);

private:
	void SetState(DTLSState state);
	void CheckProbeTimer();
	void Probe(QWORD now);
	int Send(const RTPPacket::shared& packet);
	int Send(const RTCPCompoundPacket::shared& rtcp);
	void SetRTT(DWORD rtt,QWORD now);
	void onRTCP(const RTCPCompoundPacket::shared &rtcp);
	void ReSendPacket(RTPOutgoingSourceGroup *group,WORD seq);
	DWORD SendProbe(const RTPPacket::shared& packet);
	DWORD SendProbe(RTPOutgoingSourceGroup *group,BYTE padding);
	void SendTransportWideFeedbackMessage(DWORD ssrc);
	
	int SetLocalCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	int SetRemoteCryptoSDES(const char* suite, const BYTE* key, const DWORD len);
	//Helpers
	RTPIncomingSourceGroup* GetIncomingSourceGroup(DWORD ssrc);
	RTPOutgoingSourceGroup* GetOutgoingSourceGroup(DWORD ssrc);
	RTPIncomingSource*	GetIncomingSource(DWORD ssrc);
	RTPOutgoingSource*	GetOutgoingSource(DWORD ssrc);

private:
	struct Maps
	{
		RTPMap		rtp;
		RTPMap		ext;
		RTPMap		apt;
	};
	
private:
	Sender*		sender = nullptr;
	TimeService&	timeService;
	ObjectPool<Packet>& packetPool;
	datachannels::impl::Endpoint endpoint;
	datachannels::Endpoint::Options dcOptions;
	Listener::shared listener;
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

	//TODO: change by shared pointers
	std::map<DWORD, RTPOutgoingSourceGroup*> outgoing;
	std::map<DWORD, RTPIncomingSourceGroup*> incoming;
	std::map<std::string,RTPIncomingSourceGroup*> rids;
	std::map<std::string,std::set<RTPIncomingSourceGroup*>> mids;
	CircularQueue<RTPPacket::shared> history;
	
	DWORD	mainSSRC		= 1;
	DWORD   lastMediaSSRC		= 0;
	DWORD   rtt			= 0;
	std::string iceRemoteUsername;
	std::string iceRemotePwd;
	std::string iceLocalUsername;
	std::string iceLocalPwd;
	
	Acumulator<uint32_t, uint64_t> outgoingBitrate;
	Acumulator<uint32_t, uint64_t> rtxBitrate;
	Acumulator<uint32_t, uint64_t> probingBitrate;
	
	std::map<DWORD,PacketStats> transportWideReceivedPacketsStats;
	
	std::unique_ptr<UDPDumper> dumper;
	volatile bool dumpInRTP			= false;
	volatile bool dumpOutRTP		= false;
	volatile bool dumpRTCP			= false;
	volatile bool dumpRTPHeadersOnly	= false;
	volatile bool probe			= false;
	DWORD maxProbingBitrate			= 1024*1000;
	DWORD probingBitrateLimit		= maxProbingBitrate *4;
	volatile bool senderSideEstimationEnabled = true;

	Timer::shared probingTimer;
	QWORD   lastProbe = 0;
	QWORD 	initTime = 0;
	volatile bool started = false;
	
	std::shared_ptr<SendSideBandwidthEstimation> senderSideBandwidthEstimator;
	Timer::shared sseTimer;

	bool overrideBWE = false;
	bool disableREMB = false;
	uint32_t remoteOverrideBitrate = 0;

	Timer::shared iceTimeoutTimer;
};


#endif	/* DTLSICETRANSPORT_H */

