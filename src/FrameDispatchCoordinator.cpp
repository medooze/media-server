#include "FrameDispatchCoordinator.h"

void FrameDispatchCoordinator::OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge)
{	
	// Use the address of the MediaFrameListenerBridge as stream identifier
	auto delayMs = frameDelayCalculator.OnFrame(uint64_t(&listenerBridge), now, ts, clockRate);
	
	listenerBridge.SetDelayMs(std::max(delayMs, std::chrono::milliseconds(0)));
}
