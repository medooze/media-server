#ifndef FRAMEDISPATCHCOORDINATOR_H
#define FRAMEDISPATCHCOORDINATOR_H

#include "FrameDelayCalculator.h"

#include <mutex>

class MediaFrameListenerBridge;

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

	/**
	 * Set max delay in milliseconds
	 */
	void SetMaxDelayMs(std::chrono::milliseconds maxDelayMs);

private:
	
	FrameDelayCalculator frameDelayCalculator;
	
	std::atomic<std::chrono::milliseconds> maxDelayMs;
	
	friend class TestFrameDispatchCoordinator;
};

#endif
