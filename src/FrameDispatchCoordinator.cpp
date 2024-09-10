#include "MediaFrameListenerBridge.h"
#include "FrameDispatchCoordinator.h"

FrameDispatchCoordinator::FrameDispatchCoordinator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs,
					std::shared_ptr<TimeService> timeService) :
	timeService(timeService),
	frameDelayCalculator(FrameDelayCalculator::Create(aUpdateRefsPacketLateThresholdMs, aUpdateRefsStepPacketEarlyMs, *timeService)),
	maxDelayMs(std::chrono::milliseconds(5000)) // Max delay 5 seconds
{
	static_assert(std::atomic<std::chrono::milliseconds>::is_always_lock_free);
}

void FrameDispatchCoordinator::OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge)
{
	if (ts)
	{
		auto delayMs = std::clamp(frameDelayCalculator->OnFrame(listenerBridge.GetMediaSSRC(), now, ts, clockRate),
				std::chrono::milliseconds(0), maxDelayMs.load());
	
		listenerBridge.SetDelayMs(delayMs);
	}
}

void FrameDispatchCoordinator::SetMaxDelayMs(std::chrono::milliseconds maxDelayMs)
{
	this->maxDelayMs = maxDelayMs;
}
