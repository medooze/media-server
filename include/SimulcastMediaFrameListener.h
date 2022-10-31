#ifndef SIMULCASTMEDIAFRAMELISTENER_H
#define SIMULCASTMEDIAFRAMELISTENER_H

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
	void AddMediaListener(MediaFrame::Listener* listener);
	void RemoveMediaListener(MediaFrame::Listener* listener);

	virtual void onMediaFrame(const MediaFrame& frame) { onMediaFrame(0, frame); }
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame& frame); 

	void Stop();

private:
	void Select();
	void ForwardFrame(VideoFrame& frame);
private:
	TimeService& timeService;
	DWORD ssrc = 0;
	DWORD numLayers = 0;
	DWORD extSeqNum = 0;
	Mutex mutex;
	std::set<MediaFrame::Listener*> listeners;
	std::map<DWORD, std::unique_ptr<VideoFrame>> iframes;
	std::vector<std::pair<DWORD,std::unique_ptr<VideoFrame>>> pendingFrames;
	DWORD forwarded = 0;

	uint64_t offsetTimestamp = 0;
	uint64_t firstTimestamp = 0;
	uint64_t lastTimestamp = 0;
	uint64_t lastTime = 0;
	uint64_t selectionTime = 0;
};

#endif /* SIMULCASTMEDIAFRAMELISTENER_H */

