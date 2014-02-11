#ifndef _RTPSESSION_H_
#define _RTPSESSION_H_
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <poll.h>
#include <srtp/srtp.h>
#include "config.h"
#include "use.h"
#include "rtp.h"
#include "rtpbuffer.h"
#include "remoteratecontrol.h"
#include "fecdecoder.h"
#include "stunmessage.h"
#include "remoterateestimator.h"
#include "dtls.h"

struct MediaStatistics
{
	bool		isSending;
	bool		isReceiving;
	DWORD		lostRecvPackets;
	DWORD		numRecvPackets;
	DWORD		numSendPackets;
	DWORD		totalRecvBytes;
	DWORD		totalSendBytes;
};

class RTPSession : 
	public RemoteRateEstimator::Listener,
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
		virtual void onFPURequested(RTPSession *session) = 0;
		virtual void onReceiverEstimatedMaxBitrate(RTPSession *session,DWORD bitrate) = 0;
		virtual void onTempMaxMediaStreamBitrateRequest(RTPSession *session,DWORD bitrate,DWORD overhead) = 0;
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

public:
	RTPSession(MediaFrame::Type media,Listener *listener);
	~RTPSession();
	int Init();
	void SetRemoteRateEstimator(RemoteRateEstimator* estimator);
	int SetLocalPort(int recvPort);
	int GetLocalPort();
	int SetRemotePort(char *ip,int sendPort);
	int End();

	void SetSendingRTPMap(RTPMap &map);
	void SetReceivingRTPMap(RTPMap &map);
	bool SetSendingCodec(DWORD codec);

	int SendEmptyPacket();
	int SendPacket(RTPPacket &packet,DWORD timestamp);
	int SendPacket(RTPPacket &packet);
	
	RTPPacket* GetPacket();
	void CancelGetPacket();
	DWORD GetNumRecvPackets()	const { return numRecvPackets+numRTCPRecvPackets;	}
	DWORD GetNumSendPackets()	const { return numSendPackets+numRTCPSendPackets;	}
	DWORD GetTotalRecvBytes()	const { return totalRecvBytes+totalRTCPRecvBytes;	}
	DWORD GetTotalSendBytes()	const { return totalSendBytes+totalRTCPSendBytes;	}
	DWORD GetLostRecvPackets()	const { return lostRecvPackets;	}


	MediaFrame::Type GetMediaType()	const { return media;		}

	int SetLocalCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(const char* username, const char* pwd);
	int SetProperties(const Properties& properties);
	int RequestFPU();

	int SendTempMaxMediaStreamBitrateNotification(DWORD bitrate,DWORD overhead);

	virtual void onTargetBitrateRequested(DWORD bitrate);

	virtual void onDTLSSetup(DTLSConnection::Suite suite,BYTE* localMasterKey,DWORD localMasterKeySize,BYTE* remoteMasterKey,DWORD remoteMasterKeySize);
private:
	void SetRTT(DWORD rtt);
	void Start();
	void Stop();
	int  ReadRTP();
	int  ReadRTCP();
	void ProcessRTCPPacket(RTCPCompoundPacket *packet);
	void ReSendPacket(int seq);
	int Run();
private:
	static  void* run(void *par);
protected:
	//Envio y recepcion de rtcp
	int RecvRtcp();
	int SendPacket(RTCPCompoundPacket &rtcp);
	int SendSenderReport();
	int SendFIR();
	RTCPCompoundPacket* CreateSenderReport();
private:
	typedef std::map<DWORD,RTPTimedPacket*> RTPOrderedPackets;
protected:
	RemoteRateEstimator*	remoteRateEstimator;
private:
	MediaFrame::Type media;
	Listener* listener;
	RTPBuffer packets;
	bool muxRTCP;
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
	BYTE*	sendKey;
	srtp_t	recvSRTPSession;
	srtp_t	recvSRTPSessionRTX;
	BYTE*	recvKey;

	char*	cname;
	char*	iceRemoteUsername;
	char*	iceRemotePwd;
	char*	iceLocalUsername;
	char*	iceLocalPwd;
	pthread_t thread;
	pthread_mutex_t mutex;	

	//Tipos
	int 	sendType;

	//Transmision
	sockaddr_in sendAddr;
	sockaddr_in sendRtcpAddr;
	BYTE 	sendPacket[MTU+HMACSAFEPADDING] ALIGNEDTO32;
	WORD    sendSeq;
	DWORD   sendTime;
	DWORD	sendLastTime;
	DWORD	sendSSRC;
	DWORD	sendSR;

	//Recepcion
	BYTE	recBuffer[MTU+HMACSAFEPADDING] ALIGNEDTO32;
	DWORD	recExtSeq;
	DWORD	recSSRC;
	DWORD	recTimestamp;
	timeval recTimeval;
	DWORD	recSR;
	DWORD   recCycles;
	in_addr_t recIP;
	DWORD	  recPort;

	//RTP Map types
	RTPMap* rtpMapIn;
	RTPMap* rtpMapOut;

	RTPMap	extMap;

	//Statistics
	DWORD	numRecvPackets;
	DWORD	numRTCPRecvPackets;
	DWORD	numSendPackets;
	DWORD	numRTCPSendPackets;
	DWORD	totalRecvBytes;
	DWORD	totalRTCPRecvBytes;
	DWORD	totalSendBytes;
	DWORD	totalRTCPSendBytes;
	DWORD	lostRecvPackets;
	DWORD	totalRecvPacketsSinceLastSR;
	DWORD   nackedPacketsSinceLastSR;
	DWORD	totalRecvBytesSinceLastSR;
	DWORD   minRecvExtSeqNumSinceLastSR;
	DWORD	jitter;
	
	BYTE	firReqNum;

	DWORD	rtt;
	timeval lastFPU;
	timeval lastSR;
	timeval lastReceivedSR;
	bool	requestFPU;
	bool	pendingTMBR;
	DWORD	pendingTMBBitrate;

	FECDecoder		fec;
	bool			useFEC;
	bool			useNACK;
	bool			isNACKEnabled;
	bool			useAbsTime;

	RTPOrderedPackets	rtxs;
	Use			rtxUse;
};

#endif
