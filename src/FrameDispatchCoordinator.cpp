#include "FrameDispatchCoordinator.h"

FrameDispatchCoordinator::FrameDispatchCoordinator(int aUpdateRefsPacketEarlyThresholdMs,
					int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	frameDelayCalculator(aUpdateRefsPacketEarlyThresholdMs, aUpdateRefsPacketLateThresholdMs, aUpdateRefsStepPacketEarlyMs)
{
}

void FrameDispatchCoordinator::OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge)
{	
	// Use the address of the MediaFrameListenerBridge as stream identifier
	auto delayMs = frameDelayCalculator.OnFrame(uint64_t(&listenerBridge), now, ts, clockRate);
	
	listenerBridge.SetDelayMs(std::max(delayMs, std::chrono::milliseconds(0)));
}
