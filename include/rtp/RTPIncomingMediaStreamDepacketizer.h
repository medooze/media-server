#ifndef RTPINCOMINGMEDIASTREAMDEPACKETIZER_H
#define RTPINCOMINGMEDIASTREAMDEPACKETIZER_H

#include <set>

#include "media.h"
#include "rtp/RTPIncomingMediaStream.h"
#include "RTPDepacketizer.h"
#include "use.h"

class RTPIncomingMediaStreamDepacketizer :
	public RTPIncomingMediaStream::Listener
{
public:
	RTPIncomingMediaStreamDepacketizer(const RTPIncomingMediaStream::shared& incomingSource);
	virtual ~RTPIncomingMediaStreamDepacketizer();
	
	// RTPIncomingMediaStream::Listener interface
	void onRTP(const RTPIncomingMediaStream* group,const RTPPacket::shared& packet) override;
	void onBye(const RTPIncomingMediaStream* group) override;
	void onEnded(const RTPIncomingMediaStream* group) override;
	
	void AddMediaListener(const MediaFrame::Listener::shared& listener);
	void RemoveMediaListener(const MediaFrame::Listener::shared& listener);
	
	void Stop();
	
private:
	std::set<MediaFrame::Listener::shared> listeners;
	std::unique_ptr<RTPDepacketizer> depacketizer;
	RTPIncomingMediaStream::shared incomingSource;
	TimeService &timeService;
};

#endif /* STREAMTRACKDEPACKETIZERDEPACKETIZER_H */

