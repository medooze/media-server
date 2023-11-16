#ifndef FRAMEDISPATCHCOORDINATOR_H
#define FRAMEDISPATCHCOORDINATOR_H

#include "FrameDelayCalculator.h"
#include "MediaFrameListenerBridge.h"

class FrameDispatchCoordinator
{
public:	
	static constexpr int DefaultUpdateRefsPacketLateThresholdMs = 10;
	static constexpr std::chrono::milliseconds DefaultUpdateRefsStepPacketEarlyMs = std::chrono::milliseconds(20);

	FrameDispatchCoordinator(int aUpdateRefsPacketLateThresholdMs = DefaultUpdateRefsPacketLateThresholdMs, 
			std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs = DefaultUpdateRefsStepPacketEarlyMs);
			
	/**
	 * Calculate the frame dispatching delay as per arrival time and time stamp and update the MediaFrameListenerBridge
	 */
	void OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge);

private:
	
	FrameDelayCalculator frameDelayCalculator;
	
	friend class TestFrameDispatchCoordinator;
};

#endif
