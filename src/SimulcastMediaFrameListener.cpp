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
	forwardSsrc(ssrc),
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
		//Add listener to set
		listeners.insert(listener);
		//If it is first listener
		if (listeners.size()==1)
			//For all producers
			for (auto& producer : producers)
				//Add us as listener to producer
				producer->AddMediaListener(shared_from_this());
	});
}

void SimulcastMediaFrameListener::RemoveMediaListener(const MediaFrame::Listener::shared& listener)
{
	Debug("-MediaFrameListenerBridge::RemoveListener() [this:%p,listener:%p]\n", this, listener.get());
	timeService.Sync([=](std::chrono::milliseconds){
		//Remove listener
		listeners.erase(listener);
		//If it was the last listener
		if (listeners.empty())
			//For all producers
			for (auto& producer : producers)
				//Remove us as listener to producer
				producer->RemoveMediaListener(shared_from_this());
	});
}

void SimulcastMediaFrameListener::AttachTo(const MediaFrame::Producer::shared& producer)
{
	Debug("-MediaFrameListenerBridge::AttachTo() [this:%p,producer:%p]\n", this, producer.get());

	timeService.Sync([=](std::chrono::milliseconds) {
		//Add producer
		producers.insert(producer);
		//If we have any listener
		if (!listeners.empty())
			//Add us as listener to producer
			producer->AddMediaListener(shared_from_this());
	});
}

void SimulcastMediaFrameListener::Detach(const MediaFrame::Producer::shared& producer)
{
	Debug("-MediaFrameListenerBridge::Detach() [this:%p,producer:%p]\n", this, producer.get());
	timeService.Sync([=](std::chrono::milliseconds) {
		producers.erase(producer);
	});
}

void SimulcastMediaFrameListener::Stop()
{
	timeService.Sync([this](std::chrono::milliseconds) {
		// Store remaining
		Flush();
		//For all producers
		for (auto& producer : producers)
			//Remove us as listener to producer
			producer->RemoveMediaListener(shared_from_this());
		//Clear listeners and producers
		listeners.clear();
		producers.clear();
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
		lastForwaredFrameTimeMs = 0;
		lastForwardedTimestamp.reset();

		referenceFrameTime.reset();
		initialTimestamps.clear();
		layerDimensions.clear();
		queue.clear();
		layerTimestamps.clear();
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

	if (frame.GetTime() <= lastForwaredFrameTimeMs)
	{
		// Make the time at least advance by 1ms
		frame.SetTime(lastForwaredFrameTimeMs + 1);
	}

	lastForwaredFrameTimeMs = frame.GetTime();

	frame.SetSSRC(forwardSsrc);

	//Send it to all listeners
	for (const auto& listener : listeners)
		//Send cloned frame
		listener->onMediaFrame(forwardSsrc, frame);
}

void SimulcastMediaFrameListener::Push(std::unique_ptr<VideoFrame>&& frame)
{
	DWORD ssrc = frame->GetSSRC();

	if (!referenceFrameTime)
	{
		if (!frame->IsIntra()) return;
		referenceFrameTime = frame->GetTime();
	}

	if (initialTimestamps.find(ssrc) == initialTimestamps.end())
	{
		if (!frame->IsIntra()) return;
		auto offset = (int64_t(frame->GetTime()) - int64_t(*referenceFrameTime)) * frame->GetClockRate() / 1000;
		initialTimestamps[ssrc] = frame->GetTimeStamp() - offset;
	}

	if (layerDimensions.find(ssrc) == layerDimensions.end())
	{
		if (!frame->IsIntra()) return;
		layerDimensions[ssrc] = frame->GetWidth() * frame->GetHeight();
	}

	// Convert to relative timestamp
	auto tm = frame->GetTimeStamp() > initialTimestamps[ssrc] ? frame->GetTimeStamp() - initialTimestamps[ssrc] : 0;
	frame->SetTimestamp(tm);

	// Update the layer latest timestamp
	layerTimestamps[ssrc] = tm;

	// Initially, select the first intra frame. Will later to update to higher
	// quality one if exists
	if (selectedSsrc == 0 && frame->IsIntra())
	{
		selectedSsrc = ssrc;
	}

	if (maxQueueSize == 0)
	{
		assert(numLayers == 1);
		initialised = true;
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

	// Select the best available frame in the queue during initialising
	if (!initialised && (queue.size() == maxQueueSize || layerDimensions.size() == numLayers))
	{
		assert(maxQueueSize > 0);
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
}

void SimulcastMediaFrameListener::Enqueue(std::unique_ptr<VideoFrame>&& frame)
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
// Ignore coverity error: Attempting to access the managed object of an empty smart pointer "this->queue.front()".
// coverity[dereference]
		auto tm = queue.front()->GetTimestamp();
		bool allLayersRecievedAtTimestamp = std::all_of(layerTimestamps.begin(), layerTimestamps.end(), [tm](auto& lt) {
			return lt.second >= tm;
		});

		// forward the front frame if the queue is full or the time indicates the front
		// is the earliest possible eligible frame
		if (queue.size() > maxQueueSize || allLayersRecievedAtTimestamp)
		{
			auto f = std::move(queue.front());
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
// Ignore coverity error: Attempting to access the managed object of an empty smart pointer "f".
// coverity[dereference]
		ForwardFrame(*f);
	}
}