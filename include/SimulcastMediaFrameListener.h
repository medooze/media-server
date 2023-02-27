#ifndef SIMULCASTMEDIAFRAMELISTENER_H
#define SIMULCASTMEDIAFRAMELISTENER_H

#include "SimulcastVideoMultiplexer.h"
#include "media.h"
#include "use.h"
#include "video.h"
#include "TimeService.h"

#include <map>
#include <memory>
#include <set>

class SimulcastMediaFrameListener :
	public MediaFrame::Listener
{
public:
	SimulcastMediaFrameListener(TimeService& timeService,DWORD ssrc, DWORD numLayers);
	virtual ~SimulcastMediaFrameListener();

	void SetNumLayers(DWORD numLayers);
	void AddMediaListener(const MediaFrame::Listener::shared& listener);
	void RemoveMediaListener(const MediaFrame::Listener::shared& listener);

	virtual void onMediaFrame(const MediaFrame& frame) { onMediaFrame(0, frame); }
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame& frame); 

	void Stop();

private:
	void ForwardFrame(VideoFrame& frame);
private:
	TimeService& timeService;
	DWORD ssrc = 0;
	std::set<MediaFrame::Listener::shared> listeners;

	SimulcastVideoMultiplexer selector;	
};

#endif /* SIMULCASTMEDIAFRAMELISTENER_H */

