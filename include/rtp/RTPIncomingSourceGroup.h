#ifndef RTPINCOMINGSOURCEGROUP_H
#define RTPINCOMINGSOURCEGROUP_H

#include <set>
#include <string>
#include <list>

#include "config.h"
#include "use.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPIncomingSource.h"
#include "rtp/RTPLostPackets.h"
#include "rtp/RTPBuffer.h"
#include "remoterateestimator.h"
#include "TimeService.h"

class RTPIncomingSourceGroup :
	public RemoteRateEstimator::Listener
{
public:
	class Listener 
	{
	public:
		virtual void onRTP(RTPIncomingSourceGroup* group,const RTPPacket::shared& packet) = 0;
		virtual void onEnded(RTPIncomingSourceGroup* group) = 0;
	};
public:	
	RTPIncomingSourceGroup(MediaFrame::Type type,TimeService& timeService);
	virtual ~RTPIncomingSourceGroup();
	
	RTPIncomingSource* GetSource(DWORD ssrc);
	void AddListener(Listener* listener);
	void RemoveListener(Listener* listener);
	int AddPacket(const RTPPacket::shared &packet, DWORD size = 0); //Size is only used if remb is in use
	RTPIncomingSource* Process(RTPPacket::shared &packet);
	
	
	void ResetPackets();
	void Update();
	void Update(QWORD now);
	void SetRTT(DWORD rtt);
	std::list<RTCPRTPFeedback::NACKField::shared>  GetNacks() { return losts.GetNacks(); }
	
	void Start(bool remb = false);
	void Stop();
	
	WORD SetRTTRTX(uint64_t time);
	
	DWORD GetCurrentLost()			const { return lost;		}
	DWORD GetMinWaitedTime()		const { return minWaitedTime;	}
	DWORD GetMaxWaitedTime()		const { return maxWaitedTime;	}
	long double GetAvgWaitedTime()		const {	return avgWaitedTime;	}
	
	virtual void onTargetBitrateRequested(DWORD bitrate) override;
private:
	void DispatchPackets(QWORD time);
public:	
	std::string rid;
	std::string mid;
	DWORD rtt = 0;
	MediaFrame::Type type;
	RTPIncomingSource media;
	RTPIncomingSource fec;
	RTPIncomingSource rtx;
        DWORD remoteBitrateEstimation = 0;
	
	//Stats
	DWORD lost = 0;
	DWORD minWaitedTime = 0;
	DWORD maxWaitedTime = 0;
	long double avgWaitedTime = 0;
private:
	TimeService&	timeService;
	Timer::shared	dispatchTimer;
	RTPLostPackets	losts;
	RTPBuffer	packets;
	Mutex		listenerMutex;
	std::set<Listener*>  listeners;
	RemoteRateEstimator remoteRateEstimator;
	WORD  rttrtxSeq	 = 0 ;
	QWORD rttrtxTime = 0;
	bool remb	 = false;
};

#endif /* RTPINCOMINGSOURCEGROUP_H */

