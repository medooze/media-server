#ifndef _RTPSESSION_H_
#define _RTPSESSION_H_
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <poll.h>
#include <srtp2/srtp.h>
#include "config.h"
#include "use.h"
#include "rtp.h"
#include "rtpbuffer.h"
#include "remoteratecontrol.h"
#include "fecdecoder.h"
#include "stunmessage.h"
#include "remoterateestimator.h"
#include "RTPTransport.h"

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
	public RTPTransport::Listener
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
	RTPSession(MediaFrame::Type media,Listener *listener);
	~RTPSession();
	int Init();
	void SetRemoteRateEstimator(RemoteRateEstimator* estimator);
	int SetLocalPort(int recvPort);
	int GetLocalPort();
	int SetRemotePort(char *ip,int sendPort);
	void Reset();
	int End();

	void SetSendingRTPMap(RTPMap &map);
	void SetReceivingRTPMap(RTPMap &map);
	bool SetSendingCodec(DWORD codec);

	void SendEmptyPacket();
	int SendPacket(RTPPacket &packet,DWORD timestamp);
	int SendPacket(RTPPacket &packet);

	RTPPacket* GetPacket();
	void CancelGetPacket();
	DWORD GetNumRecvPackets()	const { return recv.media.numPackets+recv.media.numRTCPPackets;	}
	DWORD GetNumSendPackets()	const { return send.media.numPackets+send.media.numRTCPPackets;	}
	DWORD GetTotalRecvBytes()	const { return recv.media.totalBytes+recv.media.totalRTCPBytes;	}
	DWORD GetTotalSendBytes()	const { return send.media.totalBytes+send.media.totalRTCPBytes;	}
	DWORD GetLostRecvPackets()	const { return recv.media.lostPackets;			}


	MediaFrame::Type GetMediaType()	const { return media;		}

	int SetLocalCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoSDES(const char* suite, const char* key64);
	int SetRemoteCryptoDTLS(const char *setup,const char *hash,const char *fingerprint);
	int SetLocalSTUNCredentials(const char* username, const char* pwd);
	int SetRemoteSTUNCredentials(const char* username, const char* pwd);
	int SetProperties(const Properties& properties);
	int RequestFPU();
	void FlushRTXPackets();

	int SendTempMaxMediaStreamBitrateNotification(DWORD bitrate,DWORD overhead);

	virtual void onTargetBitrateRequested(DWORD bitrate);

public:	
	virtual void onRemotePeer(const char* ip, const short port);
	virtual void onRTPPacket(BYTE* buffer, DWORD size);
	virtual void onRTCPPacket(BYTE* buffer, DWORD size);
private:
	void SetRTT(DWORD rtt);
	int ReSendPacket(int seq);
protected:
	//Envio y recepcion de rtcp
	int SendPacket(RTCPCompoundPacket &rtcp);
	int SendSenderReport();
	int SendFIR();
	RTCPCompoundPacket* CreateSenderReport();
private:
	typedef std::map<DWORD,RTPPacket*> RTPOrderedPackets;
protected:
	RemoteRateEstimator*	remoteRateEstimator;
private:
	MediaFrame::Type media;
	Listener* listener;
	RTPBuffer packets;
	RTPTransport transport;
	
	char*	cname;
	//Transmision
	RTPOutgoingSourceGroup send;
	RTPIncomingSourceGroup recv;

	DWORD  	sendType;
	Mutex	sendMutex;
	DWORD   apt;

	//Recepcion
	BYTE	recBuffer[MTU+SRTP_MAX_TRAILER_LEN] ALIGNEDTO32;
	DWORD	recTimestamp;
	timeval recTimeval;

	//RTP Map types
	RTPMap* rtpMapIn;
	RTPMap* rtpMapOut;

	RTPMap	extMap;

	BYTE	firReqNum;

	DWORD	rtt;
	timeval lastFPU;
	timeval lastReceivedSR;
	bool	requestFPU;
	bool	pendingTMBR;
	DWORD	pendingTMBBitrate;

	RTPLostPackets		losts;
	bool			useNACK;
	bool			useRTX;
	bool			isNACKEnabled;
	bool			useAbsTime;

	bool 			useRTCP;

	RTPOrderedPackets	rtxs;
	bool			usePLI;
};

#endif
