#include "MediaFrameListenerBridge.h"
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
		while (lock.test_and_set(std::memory_order_acq_rel));
		auto delayMs = frameDelayCalculator.OnFrame(listenerBridge.GetMediaSSRC(), now, ts, clockRate);
		lock.clear(std::memory_order_release);
	
		listenerBridge.SetDelayMs(std::max(delayMs, std::chrono::milliseconds(0)));
	}
}
