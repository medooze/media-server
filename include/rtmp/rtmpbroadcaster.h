#ifndef _RMTPBROADCASTER_H_
#define _RMTPBROADCASTER_H_

#include "config"
#include "rtmpsession.h"
#include <pthread.h>
#include <map>


class RTMPBroadcaster
{
public:
	RTPBroadcaster();

	int SendVideoFrame(BYTE* data,DWORD size,QWORD frameTime);
	int SendAudioFrame(BYTE* data,DWORD size,QWORD frameTime);

	int SendRTPVideoPacket(BYTE* data,int size,int last,long frameTime);
        int SendRTPAudioPacket(BYTE* data,int size,DWORD frameTime);

	bool AddRTMPSession(RTMPSession* rtmpSession);
	bool RemoveRTMPSession(DWORD id);

protected:
	typedef std::map<DWORD,RTMPSession*> RTMPSessions;
public:
	RTMPSessions rtmpSessions;
	DWORD maxSessionId;
	
	pthread_mutext_t mutex;
};

#endif
