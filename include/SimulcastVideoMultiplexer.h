#ifndef SIMULCASTVIDEOMULTIPLEXER_H
#define SIMULCASTVIDEOMULTIPLEXER_H

#include "log.h"
#include "video.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <deque>
#include <iostream>

class SimulcastVideoMultiplexer
{
public:
	static constexpr uint32_t MaxWaitingTimeBeforeSwitchingLayerMs = 300;
	static constexpr uint32_t MaxWaitingTimeFindingFirstBestLayerMs = 300;
	static constexpr uint32_t MaxQueueSize = 10;

	SimulcastVideoMultiplexer(uint32_t layers) : 
		numLayers(layers)
	{
	}

	template<typename T>
	void Push(std::unique_ptr<VideoFrame> frame, const T& forwardFunc)
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

		// Initially, select the first intra frame. Will later to update to higher
		// quality one if exists
		if (selectedSsrc == 0 && frame->IsIntra())
		{
			selectedSsrc = ssrc;
		}
	
		// Enqueue the selected layer frame.
		// If current frame is not from the selected layer. Switch
		// layer if following conditions met:
		// 1. The frame is intra.
		// 2. The frame is at least not earlier than the first frame in the queue.
		// 3. The frame has higher dimension or it has been too long since the current
		//    layer frame was queued.
		if (ssrc == selectedSsrc)
		{
			Enqueue(std::move(frame), forwardFunc);
		}
		else if (frame->IsIntra() && queue.front()->GetTimeStamp() <= frame->GetTimeStamp())
		{
			if (layerDimensions[ssrc] > layerDimensions[selectedSsrc] ||
			    frame->GetTime() > (lastEnqueueTimeMs + MaxWaitingTimeBeforeSwitchingLayerMs))
			{
				UltraDebug("layer switch: 0x%x -> 0x%x, time: %lld, timestamp: %lld\n", ssrc, selectedSsrc, frame->GetTime(), frame->GetTimeStamp());

				selectedSsrc = ssrc;
				Enqueue(std::move(frame), forwardFunc);
			}
		}

		// Select the best available frame in the queue during initialising
		if (!initialised && queue.size() == MaxQueueSize)
		{			 
			auto bestLayer = std::max_element(layerDimensions.begin(), layerDimensions.end(), 
				[](const auto& elementA, const auto& elementB) {
					return elementA.second < elementB.second;
				});

			selectedSsrc = bestLayer->first;
			while(!queue.empty() && queue.front()->GetSSRC() != selectedSsrc)
			{
				queue.pop_front();		
			}

			initialised = true;			
		}
	}

	template<typename T>
	void Flush(const T& forwardFunc)
	{
		while(!queue.empty())
		{
			auto f = std::move(queue.front());
			queue.pop_front();
			forwardFunc(*f);
		}
	}

private:

	template<typename T>
	void Enqueue(std::unique_ptr<VideoFrame> frame, const T& forwardFunc)
	{	
		lastEnqueueTimeMs = frame->GetTime();

		// Check the queue to remove frames newer than current one
		while(!queue.empty() && queue.back()->GetTimeStamp() >= frame->GetTimeStamp())
		{
			queue.pop_back();
		}

		queue.push_back(std::move(frame));

		// forward the front frame if the queue is full
		if (queue.size() > MaxQueueSize)
		{
			auto f = std::move(queue.front());
			f->SetSSRC(0);
			queue.pop_front();
			forwardFunc(*f);
		}
	}

	uint32_t numLayers = 0;

	bool initialised = false;
	DWORD selectedSsrc = 0;
	QWORD lastEnqueueTimeMs = 0;

	std::optional<uint64_t> referenceFrameTime;

	std::unordered_map<uint64_t, int64_t> initialTimestamps;
	std::unordered_map<uint64_t, size_t> layerDimensions;
	std::deque<std::unique_ptr<VideoFrame>> queue;
};

#endif /* SIMULCASTVIDEOMULTIPLEXER_H */