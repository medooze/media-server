#include "MediaFrameListenerBridge.h"
#include "FrameDispatchCoordinator.h"

FrameDispatchCoordinator::FrameDispatchCoordinator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	frameDelayCalculator(aUpdateRefsPacketLateThresholdMs, aUpdateRefsStepPacketEarlyMs),
	maxDelayMs(2000) // Max delay 2 seconds
{
}

void FrameDispatchCoordinator::OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge)
{
	if (ts)
	{
		while (lock.test_and_set(std::memory_order_acq_rel));
		auto delayMs = frameDelayCalculator.OnFrame(listenerBridge.GetMediaSSRC(), now, ts, clockRate);
		lock.clear(std::memory_order_release);
	
		listenerBridge.SetDelayMs(std::clamp(delayMs, std::chrono::milliseconds(0), maxDelayMs));
	}
}

void FrameDispatchCoordinator::SetMaxDelayMs(std::chrono::milliseconds maxDelayMs)
{
	while (lock.test_and_set(std::memory_order_acq_rel));
	this->maxDelayMs = maxDelayMs;
	lock.clear(std::memory_order_release);
}
