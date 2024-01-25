#ifndef RTPOUTGOINGSOURCEGROUP_H
#define RTPOUTGOINGSOURCEGROUP_H

#include <string>
#include <map>
#include <set>

#include "config.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTPOutgoingSource.h"
#include "TimeService.h"
#include "CircularBuffer.h"

struct RTPOutgoingSourceGroup
{
public:
	using shared = std::shared_ptr<RTPOutgoingSourceGroup>;
public:
	class Listener 
	{
		public:
			virtual void onPLIRequest(const RTPOutgoingSourceGroup* group,DWORD ssrc) = 0;
			virtual void onREMB(const RTPOutgoingSourceGroup* group,DWORD ssrc,DWORD bitrate) = 0;
			virtual void onEnded(const RTPOutgoingSourceGroup* group) = 0;
		
	};
public:
	RTPOutgoingSourceGroup(MediaFrame::Type type,TimeService& timeService);
	RTPOutgoingSourceGroup(const std::string &mid,MediaFrame::Type type, TimeService& timeService);
	~RTPOutgoingSourceGroup();
	
	void AddListener(Listener* listener);
	void RemoveListener(Listener* listener);
	void onPLIRequest(DWORD ssrc);
	void onREMB(DWORD ssrc,DWORD bitrate);
	TimeService& GetTimeService()	{ return timeService; }
	RTPOutgoingSource* GetSource(DWORD ssrc);
	
	void UpdateAsync(std::function<void(std::chrono::milliseconds)> callback);
	void Update(QWORD now);
	void Update();
	//RTX packets
	void AddPacket(const RTPPacket::shared& packet);
	RTPPacket::shared GetPacket(WORD seq) const;

	bool isRTXAllowed(WORD seq, QWORD now) const;
	void SetRTXTime(WORD seq, QWORD time);

	void Stop();

	bool HasForcedPlayoutDelay() const 
	{
		return forcedPlayoutDelay.has_value();
	}
	const struct RTPHeaderExtension::PlayoutDelay& GetForcedPlayoutDelay() const
	{
		
		return *forcedPlayoutDelay; 
	}
	void SetForcedPlayoutDelay(uint16_t min, uint16_t max)
	{
		forcedPlayoutDelay.emplace(min, max);
	}
	
public:	
	std::string mid;
	MediaFrame::Type type;
	RTPOutgoingSource media;
	RTPOutgoingSource fec;
	RTPOutgoingSource rtx;
	QWORD lastUpdated = 0;
private:	
	TimeService& timeService;
	CircularBuffer<RTPPacket::shared, uint16_t, 512> packets;
	CircularBuffer<QWORD, uint16_t, 512> rtxTimes;
	std::set<Listener*> listeners;
	std::optional<struct RTPHeaderExtension::PlayoutDelay> forcedPlayoutDelay;
};


#endif /* RTPOUTGOINGSOURCEGROUP_H */

