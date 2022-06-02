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
	class Listener 
	{
		public:
			virtual void onPLIRequest(RTPOutgoingSourceGroup* group,DWORD ssrc) = 0;
			virtual void onREMB(RTPOutgoingSourceGroup* group,DWORD ssrc,DWORD bitrate) = 0;
			virtual void onEnded(RTPOutgoingSourceGroup* group) = 0;
		
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
	
	void Update(QWORD now);
	void Update();
	//RTX packets
	void AddPacket(const RTPPacket::shared& packet);
	RTPPacket::shared GetPacket(WORD seq) const;

	void Stop();
	
	
public:	
	std::string mid;
	MediaFrame::Type type;
	RTPOutgoingSource media;
	RTPOutgoingSource rtx;
	QWORD lastUpdated = 0;
private:	
	TimeService& timeService;
	CircularBuffer<RTPPacket::shared, uint16_t, 512> packets;
	std::set<Listener*> listeners;
};


#endif /* RTPOUTGOINGSOURCEGROUP_H */

