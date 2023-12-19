#include "FrameDispatchCoordinator.h"

FrameDispatchCoordinator::FrameDispatchCoordinator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	frameDelayCalculator(aUpdateRefsPacketLateThresholdMs, aUpdateRefsStepPacketEarlyMs)
{
}

void FrameDispatchCoordinator::OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge)
{
	if (ts)
	{
		auto delayMs = frameDelayCalculator.OnFrame(listenerBridge.GetMediaSSRC(), now, ts, clockRate);
	
		listenerBridge.SetDelayMs(std::max(delayMs, std::chrono::milliseconds(0)));
	}
}
