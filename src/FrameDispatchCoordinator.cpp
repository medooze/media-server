#include "MediaFrameListenerBridge.h"
#include "FrameDispatchCoordinator.h"

FrameDispatchCoordinator::FrameDispatchCoordinator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	frameDelayCalculator(aUpdateRefsPacketLateThresholdMs, aUpdateRefsStepPacketEarlyMs),
	maxDelayMs(5000) // Max delay 5 seconds
{
}

void FrameDispatchCoordinator::OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge)
{
	if (ts)
	{
		std::chrono::milliseconds delayMs;
		
		{
			std::lock_guard<std::mutex> lock(mutex);
			// @todo I think we should NOT clamp here but instead use the MFLB to clamp which already does max check. This means we pass back to MFLB what we thing the desired delay is and it forces it into a valid range. It will aloow better logging of the calced values
			delayMs = std::clamp(frameDelayCalculator.OnFrame(listenerBridge.GetMediaSSRC(), now, ts, clockRate),
					std::chrono::milliseconds(0), maxDelayMs);
		}
	
		listenerBridge.SetDelayMs(delayMs);
	}
}

void FrameDispatchCoordinator::SetMaxDelayMs(std::chrono::milliseconds maxDelayMs)
{
	std::lock_guard<std::mutex> lock(mutex);
	
	this->maxDelayMs = maxDelayMs;
}
