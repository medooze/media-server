#ifndef RTPINCOMINGSOURCEGROUP_H
#define RTPINCOMINGSOURCEGROUP_H

#include <set>
#include <string>
#include <list>
#include <optional>

#include "config.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPIncomingMediaStream.h"
#include "rtp/RTPIncomingSource.h"
#include "rtp/RTPLostPackets.h"
#include "rtp/RTPBuffer.h"
#include "remoterateestimator.h"
#include "TimeService.h"

class RTPIncomingSourceGroup :
	public RTPIncomingMediaStream,
	public RemoteRateEstimator::Listener
{
public:
	using shared = std::shared_ptr<RTPIncomingSourceGroup>;
public:	
	RTPIncomingSourceGroup(MediaFrame::Type type,TimeService& timeService);
	virtual ~RTPIncomingSourceGroup();
	
	RTPIncomingSource* GetSource(DWORD ssrc);
	virtual void AddListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual void RemoveListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual DWORD GetMediaSSRC() const	override { return media.ssrc;	}
	virtual TimeService& GetTimeService()	override { return timeService;	}
	virtual void Mute(bool muting);
	int AddPacket(const RTPPacket::shared &packet, DWORD size, QWORD now);
	RTPIncomingSource* Process(RTPPacket::shared &packet);
	void Bye(DWORD ssrc);
	
	void SetRTXEnabled(bool enabled);
	void SetMaxWaitTime(DWORD maxWaitingTime);
	void ResetMaxWaitTime();
	void ResetPackets();
	void Update();
	void UpdateAsync(std::function<void(std::chrono::milliseconds)> callback);
	void SetRTT(DWORD rtt, QWORD now);
	std::list<RTCPRTPFeedback::NACKField::shared>  GetNacks() { return losts.GetNacks(); }
	
	void Start(bool remb = false);
	void Stop();
	
	WORD SetRTTRTX(uint64_t time);
	
	DWORD GetCurrentLost()			const { return losts.GetTotal();}
	DWORD GetMinWaitedTime()		const { return minWaitedTime;	}
	DWORD GetMaxWaitedTime()		const { return maxWaitedTime;	}
	long double GetAvgWaitedTime()		const {	return avgWaitedTime;	}
	
	virtual void onTargetBitrateRequested(DWORD bitrate, DWORD bandwidthEstimation, DWORD targetBitrate) override;
private:
	void DispatchPackets(QWORD time);
public:	
	std::string rid;
	std::string mid;
	DWORD rtt = 0;
	MediaFrame::Type type;
	RTPIncomingSource media;
	RTPIncomingSource rtx;
        DWORD remoteBitrateEstimation = 0;
	
	//Stats
	DWORD lost = 0;
	DWORD minWaitedTime = 0;
	DWORD maxWaitedTime = 0;
	long double avgWaitedTime = 0;
	
	QWORD lastUpdated = 0;
	BYTE codec = -1;

	//TODO: FIx
	RemoteRateEstimator remoteRateEstimator;
private:
	TimeService&	timeService;
	Timer::shared	dispatchTimer;
	RTPLostPackets	losts;
	RTPBuffer	packets;
	std::set<RTPIncomingMediaStream::Listener*>  listeners;
	std::optional<std::vector<bool>> activeDecodeTargets;
	std::optional<TemplateDependencyStructure> templateDependencyStructure;
	
	bool  isRTXEnabled = true;
	WORD  rttrtxSeq	 = 0 ;
	QWORD rttrtxTime = 0;
	bool remb	 = false;
	std::optional<DWORD> maxWaitingTime;
	volatile bool muted = false;
	
};

#endif /* RTPINCOMINGSOURCEGROUP_H */

