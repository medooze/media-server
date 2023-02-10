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
	RTPIncomingMediaStreamMultiplexer(const RTPIncomingMediaStream::shared& incomingMediaStream, TimeService& timeService);
	virtual ~RTPIncomingMediaStreamMultiplexer() = default;
	
	// RTPIncomingMediaStream interface;
	virtual void AddListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual void RemoveListener(RTPIncomingMediaStream::Listener* listener) override;
	virtual DWORD GetMediaSSRC() const override { return incomingMediaStream ? incomingMediaStream->GetMediaSSRC() : 0; }
	virtual void Mute(bool muting);

	// RTPIncomingMediaStream::Listener interface
	virtual void onRTP(const RTPIncomingMediaStream* stream, const RTPPacket::shared& packet) override;
	virtual void onRTP(const RTPIncomingMediaStream* stream, const std::vector<RTPPacket::shared>& packets) override;
	virtual void onBye(const RTPIncomingMediaStream* stream) override;
	virtual void onEnded(const RTPIncomingMediaStream* stream) override;

	virtual TimeService& GetTimeService() override { return timeService; }
	void Stop();
private:
	RTPIncomingMediaStream::shared incomingMediaStream;
	TimeService& timeService;
	std::set<RTPIncomingMediaStream::Listener*>  listeners;
	volatile bool muted = false;
};

#endif //RTPINCOMINGMEDIASTREAMMULTIPLEXER_H
