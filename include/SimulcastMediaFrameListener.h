#ifndef SIMULCASTMEDIAFRAMELISTENER_H
#define SIMULCASTMEDIAFRAMELISTENER_H

#include "media.h"
#include "use.h"
#include "video.h"
#include "TimeService.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <unordered_set>
#include <unordered_map>

class SimulcastMediaFrameListener :
	public TimeServiceWrapper<SimulcastMediaFrameListener>,
	public MediaFrame::Listener,
	public MediaFrame::Producer
{
private:
	// Private constructor to prevent creating without TimeServiceWrapper::Create() factory
	friend class TimeServiceWrapper<SimulcastMediaFrameListener>;
	SimulcastMediaFrameListener(TimeService& timeService,DWORD ssrc, DWORD numLayers);

public:
	virtual ~SimulcastMediaFrameListener();

	void SetNumLayers(DWORD numLayers);

	//MediaFrame::Producer interface
	virtual void AddMediaListener(const MediaFrame::Listener::shared& listener) override;
	virtual void RemoveMediaListener(const MediaFrame::Listener::shared& listener) override;

	//MediaFrame::Listener interface
	virtual void onMediaFrame(const MediaFrame& frame) override { onMediaFrame(frame.GetSSRC(), frame); };
	virtual void onMediaFrame(DWORD ssrc, const MediaFrame& frame) override;


	void AttachTo(const MediaFrame::Producer::shared& producer);
	void Detach(const MediaFrame::Producer::shared& producer);

	void Stop();

private:
	//To be run on timerService thread
	void PushAsync(std::chrono::milliseconds now, std::shared_ptr<VideoFrame>&& frame);

	void ForwardFrame(VideoFrame& frame);

	void Push(std::shared_ptr<VideoFrame>&& frame);
	void Enqueue(std::shared_ptr<VideoFrame>&& frame);
	void Flush();
private:
	DWORD forwardSsrc = 0;
	std::unordered_set<MediaFrame::Listener::shared> listeners;
	std::unordered_set<MediaFrame::Producer::shared> producers;

	uint32_t numLayers = 0;
	uint32_t maxQueueSize = 0;

	bool initialised = false;
	DWORD selectedSsrc = 0;
	QWORD lastEnqueueTimeMs = 0;
	QWORD lastForwaredFrameTimeMs = 0;
	std::optional<QWORD> lastForwardedTimestamp;

	std::optional<uint64_t> referenceFrameTime;

	std::unordered_map<uint64_t, int64_t> initialTimestamps;
	std::unordered_map<uint64_t, size_t> layerDimensions;
	std::deque<std::shared_ptr<VideoFrame>> queue;

	// Latest timestamps for each layer
	std::unordered_map<uint32_t, uint64_t> layerTimestamps;
};

#endif /* SIMULCASTMEDIAFRAMELISTENER_H */
