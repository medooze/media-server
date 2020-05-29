#ifndef RTPINCOMINGMEDIASTREAMMULTIPLEXER_H
#define RTPINCOMINGMEDIASTREAMMULTIPLEXER_H

#include <set>

#include "config.h"
#include "use.h"
#include "rtp/RTPIncomingMediaStream.h"
#include "TimeService.h"


class RTPIncomingMediaStreamMultiplexer : 
	public RTPIncomingMediaStream,
	public RTPIncomingMediaStream::Listener
{
public:
	RTPIncomingMediaStreamMultiplexer(DWORD ssrc, TimeService& timeService);
	virtual ~RTPIncomingMediaStreamMultiplexer() = default;
	virtual void AddListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual void RemoveListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual DWORD GetMediaSSRC() override { return ssrc; }
	
	virtual void onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet) override;
	virtual void onRTP(RTPIncomingMediaStream* stream,const std::vector<RTPPacket::shared>& packets) override;
	virtual void onBye(RTPIncomingMediaStream* stream) override;
	virtual void onEnded(RTPIncomingMediaStream* stream) override;
	virtual TimeService& GetTimeService() override { return timeService; }
	void Stop();
private:
	DWORD		ssrc = 0;
	TimeService&	timeService;
	std::set<RTPIncomingMediaStream::Listener*>  listeners;	
};

#endif /* RTPINCOMINGMEDIASTREAMMULTIPLEXER_H */

