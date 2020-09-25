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
	RTPIncomingMediaStreamDepacketizer(RTPIncomingMediaStream* incomingSource);
	virtual ~RTPIncomingMediaStreamDepacketizer();
	
	void onRTP(RTPIncomingMediaStream* group,const RTPPacket::shared& packet) override;
	void onBye(RTPIncomingMediaStream* group) override;
	void onEnded(RTPIncomingMediaStream* group) override;
	
	void AddMediaListener(MediaFrame::Listener *listener);
	void RemoveMediaListener(MediaFrame::Listener *listener);
	
	void Stop();
	
private:
	std::set<MediaFrame::Listener*> listeners;
	RTPDepacketizer* depacketizer;
	RTPIncomingMediaStream* incomingSource;
	Mutex mutex;
};

#endif /* STREAMTRACKDEPACKETIZERDEPACKETIZER_H */

