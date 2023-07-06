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
	static constexpr int DefaultUpdateRefsPacketEarlyThresholdMs = -200;
	static constexpr int DefaultUpdateRefsPacketLateThresholdMs = 0;	
	static constexpr std::chrono::milliseconds DefaultUpdateRefsStepPacketEarlyMs = std::chrono::milliseconds(20);

	FrameDelayCalculator(int aUpdateRefsPacketEarlyThresholdMs = DefaultUpdateRefsPacketEarlyThresholdMs,
			int aUpdateRefsPacketLateThresholdMs = DefaultUpdateRefsPacketLateThresholdMs, 
			std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs = DefaultUpdateRefsStepPacketEarlyMs);
	
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
	
	// Controls the stability of the dispatching delay.
	int updateRefsPacketEarlyThresholdMs = 0;	
	// Controls how accurate the a/v sync. Smaller means more in sync. Must >= 0. 
	int updateRefsPacketLateThresholdMs = 0;
	// Controls the latency reduction speed
	std::chrono::milliseconds updateRefsStepPacketEarlyMs {0};
	 
	bool initialized = false;
	bool reducingLatency = false;
	
	std::chrono::milliseconds refTime {0};
	uint64_t refTimestamp = 0;
	
	std::unordered_map<uint64_t, std::pair<std::chrono::milliseconds, uint64_t>> frameArrivalInfo;
	
	friend class TestFrameDelayCalculator;
};

#endif
