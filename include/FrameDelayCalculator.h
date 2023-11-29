#ifndef FRAMEDELAYCALCULATOR_H
#define FRAMEDELAYCALCULATOR_H

#include "media.h"
#include "rtp/RTPPacket.h"
#include "MediaFrameListenerBridge.h"
#include <chrono>
#include <queue>

class FrameDelayCalculator
{
public:

	FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs);
	
	/**
	 * Calculate the dispatch delay for the arrived frame
	 * 
	 * @param streamIdentifier 	The stream identifier
	 * @param now 	The current time (frame arrival)
	 * @param ts 	The frame timestamp
	 * @param clockRate The clock rate of the timestamp
	 * 
	 * @return The delay against the arrivial time for dispatching the frame, 
	 */
	std::chrono::milliseconds OnFrame(uint64_t streamIdentifier, std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate);

private:
	
	/**
	 * Get the delay of current frame
	 * 
	 * @param now The current time
	 * @param unifiedTs The unified timestamp
	 * 
	 * @return The delay of current frame, which is value of current time minus scheduled time for the packet, in milliseconds.
	 */
	int64_t GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs) const;
	
	// The following thresholds are checking the offsets of arrival time with regard to the previously
	// scheduled time.
	
	// Controls how accurate the a/v sync. Smaller means more in sync. Must >= 0. 
	int updateRefsPacketLateThresholdMs = 0;
	// Controls the latency reduction speed
	std::chrono::milliseconds updateRefsStepPacketEarlyMs {0};
	 
	bool initialized = false;
	
	// The start time when all streams arrive early. If packets become late, this time will be reset.
	std::optional<std::chrono::milliseconds> allEarlyStartTimeMs;
	
	std::chrono::milliseconds refTime {0};
	uint64_t refTimestamp = 0;
	
	std::unordered_map<uint64_t, std::pair<std::chrono::milliseconds, uint64_t>> frameArrivalInfo;
	
	friend class TestFrameDelayCalculator;
};

#endif
