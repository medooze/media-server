#ifndef FRAMEDELAYCALCULATOR_H
#define FRAMEDELAYCALCULATOR_H

#include "TimeService.h"

#include <unordered_map>
#include <optional>
#include <chrono>
#include <queue>
#include <algorithm>

class FrameDelayCalculator : public std::enable_shared_from_this<FrameDelayCalculator>
{
public:

	FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs, TimeService& timeService);
	
	/**
	 * Calculate the dispatch delay for the arrived frame. This function is thread safe.
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
	
	class Reference
	{
	public:
		std::pair<std::chrono::milliseconds, uint64_t> Get();
		void Set(std::chrono::milliseconds refTime, uint64_t refTimestamp);
		
	private:
		union alignas (16) ReferenceField
		{
			struct
			{
				int64_t refTime;
				uint64_t refTimestamp;
			} content;
			__uint128_t value;
		};
		
		__uint128_t field = 0;
	};
	
	/**
	 * Get the delay of current frame
	 * 
	 * @param now The current time
	 * @param unifiedTs The unified timestamp
	 * @param refTime The reference time
	 * @param refTimestamp The reference timestamp
	 * 
	 * @return The delay of current frame, which is value of current time minus scheduled time for the packet, in milliseconds.
	 */
	static int64_t GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs, std::chrono::milliseconds refTime, uint64_t refTimestamp);
	
	/**
	 * Timeservice for latency reduction
	 */
	TimeService& timeService;
	
	// The following thresholds are checking the offsets of arrival time with regard to the previously
	// scheduled time.
	
	// Controls how accurate the a/v sync. Smaller means more in sync. Must >= 0. 
	int updateRefsPacketLateThresholdMs = 0;
	// Controls the latency reduction speed
	std::chrono::milliseconds updateRefsStepPacketEarlyMs {0};
	
	// Time reference
	Reference reference;

	// The start time when all streams arrive early. If packets become late, this time will be reset.
	std::optional<std::chrono::milliseconds> allEarlyStartTimeMs;
	
	std::unordered_map<uint64_t, std::pair<std::chrono::milliseconds, uint64_t>> frameArrivalInfo;
	
	friend class TestFrameDelayCalculator;
};

#endif
