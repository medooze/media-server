#ifndef _RTPSESSION_H_
#define _RTPSESSION_H_
#include "config.h"
#include "codecs.h"
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <use.h>
#include <poll.h>
#include "rtp.h"
#include "waitqueue.h"

typedef std::map<DWORD,DWORD> RTPMap;

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

class RTPSession
{
public:
	class Listener
	{
	public:
		virtual void onFPURequested(RTPSession *session) = 0;
	};
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
	DWORD GetNumRecvPackets()	{ return numRecvPackets;	}
	DWORD GetNumSendPackets()	{ return numSendPackets;	}
	DWORD GetTotalRecvBytes()	{ return totalRecvBytes;	}
	DWORD GetTotalSendBytes()	{ return totalSendBytes;	}
	DWORD GetLostRecvPackets()	{ return lostRecvPackets;	}

	WORD  GetLost()			{ return lost;			} //TODO: ugly, will be fixed
private:
	void Start();
	void Stop();
	void ReadRTP();
	void ReadRTCP();
	void SetSendingType(int type);
	int Run();
private:
	static  void* run(void *par);
protected:
	//Envio y recepcion de rtcp
	int RecvRtcp();
	int SendRtcp();

private:
	MediaFrame::Type media;
	Listener* listener;
	WaitQueue<RTPPacket*> packets;
	//Sockets
	int 	simSocket;
	int 	simRtcpSocket;
	int 	simPort;
	int	simRtcpPort;
	pollfd	ufds[2];
	bool	inited;
	bool	running;
	WORD	lost;

	pthread_t thread;
	pthread_mutex_t mutex;	

	//Tipos
	int 	sendType;

	//Transmision
	sockaddr_in sendAddr;
	sockaddr_in sendRtcpAddr;
	BYTE 	sendPacket[MTU];
	WORD    sendSeq;
	DWORD   sendTime;
	DWORD	sendSSRC;

	//Recepcion
	BYTE	recBuffer[MTU];
	int	recSeq;
	DWORD	recSSRC;
	in_addr_t recIP;
	DWORD	  recPort;

	//RTP Map types
	RTPMap* rtpMapIn;
	RTPMap* rtpMapOut;

	//Statistics
	DWORD		numRecvPackets;
	DWORD		numSendPackets;
	DWORD		totalRecvBytes;
	DWORD		totalSendBytes;
	DWORD		lostRecvPackets;

	
};

#endif
