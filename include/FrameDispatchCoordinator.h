#ifndef FRAMEDISPATCHCOORDINATOR_H
#define FRAMEDISPATCHCOORDINATOR_H

#include "FrameDelayCalculator.h"
#include "MediaFrameListenerBridge.h"

class FrameDispatchCoordinator
{
public:	
	/**
	 * Calculate the frame dispatching delay as per arrival time and time stamp and update the MediaFrameListenerBridge
	 */
	void OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge);

private:
	
	FrameDelayCalculator frameDelayCalculator;
	
	friend class TestFrameDispatchCoordinator;
};

#endif
