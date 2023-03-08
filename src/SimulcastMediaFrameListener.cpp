#include "SimulcastMediaFrameListener.h"

namespace {

static constexpr uint32_t MaxWaitingTimeBeforeSwitchingLayerMs = 300;
static constexpr uint32_t MaxWaitingTimeFindingFirstBestLayerMs = 300;

// A factor to calculate the max queue size basing on the number of layers.
// If there is only one layer, no queue is required. Otherwise, every 
// additional layer increases the max queue size by this value
static constexpr uint32_t MaxQueueSizeFactor = 5;

constexpr uint32_t calcMaxQueueSize(DWORD numLayers)
{
	return numLayers > 0 ? (numLayers - 1) * MaxQueueSizeFactor : 0;
} 
}

SimulcastMediaFrameListener::SimulcastMediaFrameListener(TimeService& timeService, DWORD ssrc, DWORD numLayers) :
	timeService(timeService),
	ssrc(ssrc),
	numLayers(numLayers),
	maxQueueSize(calcMaxQueueSize(numLayers))
{
}

SimulcastMediaFrameListener::~SimulcastMediaFrameListener()
{
}

void SimulcastMediaFrameListener::AddMediaListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-MediaFrameListenerBridge::AddListener() [this:%p,listener:%p]\n", this, listener.get());
	
	timeService.Sync([=](std::chrono::milliseconds){
		listeners.insert(listener);
	});
}

void SimulcastMediaFrameListener::RemoveMediaListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-MediaFrameListenerBridge::RemoveListener() [this:%p,listener:%p]\n", this, listener.get());
	timeService.Sync([=](std::chrono::milliseconds){
		listeners.erase(listener);
	});
}

void SimulcastMediaFrameListener::Stop()
{
	timeService.Sync([this](std::chrono::milliseconds) {
		// Store remaining
		Flush();

		//Clear listeners
		listeners.clear();
	});
}

void SimulcastMediaFrameListener::SetNumLayers(DWORD numLayers)
{
	timeService.Sync([this, numLayers](std::chrono::milliseconds) {
		this->numLayers = numLayers;
		maxQueueSize = calcMaxQueueSize(numLayers);
		initialised = false;
		selectedSsrc = 0;
		lastEnqueueTimeMs = 0;
		lastForwardedTimestamp.reset();

		referenceFrameTime.reset();
		initialTimestamps.clear();
		layerDimensions.clear();
		queue.clear();
		timestampLayers.clear();
	});
}


void SimulcastMediaFrameListener::onMediaFrame(DWORD ssrc, const MediaFrame& frame)
{
	//Ensure it is video
	if (frame.GetType()!=MediaFrame::Video)
		//Uh?
		return;

	//Get cloned video frame
	auto cloned = std::unique_ptr<VideoFrame>((VideoFrame*)frame.Clone());
	cloned->SetSSRC(ssrc);

	Push(std::move(cloned));
}

void SimulcastMediaFrameListener::ForwardFrame(VideoFrame& frame)
{
	//Debug
	//UltraDebug("-SimulcastMediaFrameListener::ForwardFrame() | Forwarding frame [ts:%llu]\n", frame.GetTimestamp());

	//Send it to all listeners
	for (const auto& listener : listeners)
		//Send cloned frame
		listener->onMediaFrame(ssrc, frame);
}

void SimulcastMediaFrameListener::Push(std::unique_ptr<VideoFrame> frame)
{
	DWORD ssrc = frame->GetSSRC();

	if (!referenceFrameTime)
	{
		referenceFrameTime = frame->GetTime();
	}

	if (initialTimestamps.find(ssrc) == initialTimestamps.end())
	{
		auto offset = (int64_t(frame->GetTime()) - int64_t(*referenceFrameTime)) * frame->GetClockRate() / 1000;
		initialTimestamps[ssrc] = frame->GetTimeStamp() - offset;
	}
	
	if (layerDimensions.find(ssrc) == layerDimensions.end())
	{
		layerDimensions[ssrc] = frame->GetWidth() * frame->GetHeight();
	}

	// Convert to relative timestamp
	auto tm = frame->GetTimeStamp() > initialTimestamps[ssrc] ? frame->GetTimeStamp() - initialTimestamps[ssrc] : 0;
	frame->SetTimestamp(tm);

	// Update the timestamp layer information list.
	bool updated = false;
	for (auto it = timestampLayers.begin(); !updated && it != timestampLayers.end(); ++it)
	{
		// Add the ssrc for timestamp that is no later than current
		if (it->first <= tm)
		{
			it->second.insert(frame->GetSSRC());

			if (it->first == tm)
			{
				// Found matching timestamp, just break
				updated = true;
				break;
			}
		}
		else
		{
			// Couldn't find same timestamp, insert it and copy ssrcs 
			// from the later timestamp
			std::set<uint32_t> ssrcs = {frame->GetSSRC()};
			ssrcs.insert(it->second.begin(), it->second.end());
			timestampLayers.emplace(it, tm, ssrcs);
			updated = true;
			break;
		}
	}		

	if (!updated)
	{
		// If current frame is newer, append the timestamp to the end
		timestampLayers.emplace_back(tm, std::set{frame->GetSSRC()});
	}

	// Initially, select the first intra frame. Will later to update to higher
	// quality one if exists
	if (selectedSsrc == 0 && frame->IsIntra())
	{
		selectedSsrc = ssrc;
	}

	if (!initialised)
	{			 
		// Select the best available frame in the queue during initialising
		if (maxQueueSize > 0 && queue.size() == maxQueueSize)
		{
			auto bestLayerFrame = std::max_element(queue.begin(), queue.end(), 
				[this](const auto& elementA, const auto& elementB) {
					return layerDimensions[elementA->GetSSRC()] < layerDimensions[elementB->GetSSRC()];
				});

			selectedSsrc = (*bestLayerFrame)->GetSSRC();
			while(!queue.empty() && queue.front()->GetSSRC() != selectedSsrc)
			{
				queue.pop_front();		
			}

			initialised = true;
		}
		else if (maxQueueSize == 0)
		{
			initialised = true;		
		}
	}

	// Enqueue the selected layer frame.
	// If current frame is not from the selected layer. Switch
	// layer if following conditions met:
	// 1. The frame is intra.
	// 2. The frame is at least later than the last forwarded frame.
	// 3. The frame has higher dimension or it has been too long since the current
	//    layer frame was queued.
	if (ssrc == selectedSsrc)
	{
		Enqueue(std::move(frame));
	}
	else if (frame->IsIntra() && (!lastForwardedTimestamp || frame->GetTimeStamp() > *lastForwardedTimestamp))
	{
		if (layerDimensions[ssrc] > layerDimensions[selectedSsrc] ||
			frame->GetTime() > (lastEnqueueTimeMs + MaxWaitingTimeBeforeSwitchingLayerMs))
		{
			//UltraDebug("layer switch: 0x%x -> 0x%x, time: %lld, timestamp: %lld\n", ssrc, selectedSsrc, frame->GetTime(), tm);

			selectedSsrc = ssrc;
			Enqueue(std::move(frame));
		}
	}
}

void SimulcastMediaFrameListener::Enqueue(std::unique_ptr<VideoFrame> frame)
{	
	lastEnqueueTimeMs = frame->GetTime();

	// Check the queue to remove frames newer than current one
	while(!queue.empty() && queue.back()->GetTimeStamp() >= frame->GetTimeStamp())
	{
		queue.pop_back();
	}

	queue.push_back(std::move(frame));

	while (!queue.empty() && initialised)
	{
		assert(!timestampLayers.empty());
		// Remove the earlier timestamp information
		while (queue.front()->GetTimeStamp() > timestampLayers.front().first)
		{
			timestampLayers.pop_front();
		}

		assert(!timestampLayers.empty());
		// forward the front frame if the queue is full or the time indicates the front
		// is the earliest possible eligible frame
		if (queue.size() > maxQueueSize || timestampLayers.front().second.size() == numLayers)
		{
			auto f = std::move(queue.front());
			f->SetSSRC(0);
			queue.pop_front();
			ForwardFrame(*f);
			lastForwardedTimestamp = f->GetTimeStamp();
		}
		else 
		{
			break;
		}
	}
}

void SimulcastMediaFrameListener::Flush()
{
	while(!queue.empty())
	{
		auto f = std::move(queue.front());
		queue.pop_front();
		ForwardFrame(*f);
	}
}