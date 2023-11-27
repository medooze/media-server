#include "FrameDelayCalculator.h"
#include <iostream>
namespace
{

/**
 * Convert timestamp from one clock rate to another
 * 
 * @param ts The input timestamp
 * @param originalRate The clock rate of the input timestamp
 * @param targetRate The target clock rate
 * 
 * @return The timestamp basing on the target clock rate
 */
template<typename T>
static constexpr T convertTimestampClockRate(T ts, uint64_t originalRate, uint64_t targetRate)
{
	static_assert(sizeof(T) >= 8);
	return originalRate == targetRate ? ts : (ts * T(targetRate) / T(originalRate));
}

static constexpr uint64_t UnifiedClockRate = 90 * 1000;
}

FrameDelayCalculator::FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	updateRefsPacketLateThresholdMs(aUpdateRefsPacketLateThresholdMs),
	updateRefsStepPacketEarlyMs(aUpdateRefsStepPacketEarlyMs)
{
}

std::chrono::milliseconds FrameDelayCalculator::OnFrame(uint64_t streamIdentifier, std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate)
{	
	//Log("S: %lld, now: %lld, ts: %lld, clk: %lld\n", streamIdentifier, now.count(), ts, clockRate);
	
	if (ts == 0) return std::chrono::milliseconds(0);
	
	auto unifiedTs = convertTimestampClockRate(ts, clockRate, UnifiedClockRate);
		
	if (!initialized)
	{
		refTime = now;
		refTimestamp = unifiedTs;
		
		initialized = true;			
		return std::chrono::milliseconds(0);
	}
			
	// Note the lateMs could be negative when the frame arrives earlier than scheduled	
	auto lateMs = GetFrameArrivalDelayMs(now, unifiedTs);
	
	// We would delay the early arrived frame
	std::chrono::milliseconds delayMs(-lateMs);
	auto updateRefsPacketEarlyThresholdMs = -updateRefsStepPacketEarlyMs.count();
	
	bool allEarly = false;
	if (lateMs > updateRefsPacketLateThresholdMs)  // Packet late
	{
		refTime = now;
		refTimestamp = unifiedTs;
		
		delayMs = std::chrono::milliseconds(0);
	}
	else if (lateMs < updateRefsPacketEarlyThresholdMs)  // Packet early
	{
		// Loop to see if all the streams have arrived earlier
		allEarly = std::all_of(frameArrivalInfo.begin(), frameArrivalInfo.end(), 
			[&](const auto& info) {
				if (info.first == streamIdentifier) return true;
							
				auto [time, timestamp] = info.second;
				auto frameLateMs = GetFrameArrivalDelayMs(time, timestamp);
				
				return frameLateMs < updateRefsPacketEarlyThresholdMs;
			});
	}
			
	if (allEarly)
	{
		if (!allEarlyStartTimeMs.has_value())
			allEarlyStartTimeMs = now;
	}
	else
	{
		allEarlyStartTimeMs.reset();
	}


	frameArrivalInfo[streamIdentifier] = {now, unifiedTs};
	
	// If all stream becomes ealier for a while (> 2s), we reduce the latency
	if (allEarlyStartTimeMs.has_value() && (now - *allEarlyStartTimeMs) > 2000ms)
	{
		// Make reference time earlier for same time stamp, which means
		// frames will be dispatched ealier.
		refTime -= updateRefsStepPacketEarlyMs;
		
		// Reduce the delay
		delayMs -= updateRefsStepPacketEarlyMs;
		
		// Restart the latency reduction process to have a max reduction rate
		// at 20ms per second as we don't expect the latency would be too large,
		// which normall would be below 1 second.
		allEarlyStartTimeMs.reset();
	}
	
	return std::max(delayMs, std::chrono::milliseconds(0));
}

int64_t FrameDelayCalculator::GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs) const
{	
	constexpr int64_t MsToTimestampFactor = UnifiedClockRate / 1000;
	auto scheduledMs = (int64_t(unifiedTs) - int64_t(refTimestamp)) / MsToTimestampFactor + refTime.count();
	auto actualTimeMs = now.count();

	return actualTimeMs - scheduledMs;
}