#include "FrameDelayCalculator.h"
#include <iostream>
#include <chrono>
#include "log.h"

using namespace std::chrono;

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
static constexpr uint64_t MaxClockDesync = 100 * UnifiedClockRate; //100s
}

FrameDelayCalculator::FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	updateRefsPacketLateThresholdMs(aUpdateRefsPacketLateThresholdMs),
	updateRefsStepPacketEarlyMs(aUpdateRefsStepPacketEarlyMs)
{
}

std::chrono::milliseconds FrameDelayCalculator::OnFrame(uint64_t streamIdentifier, std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate)
{	
	//Log("-FrameDelayCalculator::OnFrame() | [streamIdentifier:%lld,now:%lld,ts:%lld,clockRate:%lld,refTime:%lld,refTimestamp:%lld]\n", streamIdentifier, now.count(), ts, clockRate, refTime, refTimestamp);
	
	if (ts == 0) return 0ms;
	
	auto unifiedTs = convertTimestampClockRate(ts, clockRate, UnifiedClockRate);
		
	if (!initialized)
	{
		refTime = now;
		refTimestamp = unifiedTs;
		
		initialized = true;			
		return 0ms;
	}
			
	// Note the lateMs could be negative when the frame arrives earlier than scheduled	
	auto lateMs = GetFrameArrivalDelayMs(now, unifiedTs);

	//Calculate the timestamp diff to ensure we are using same 
	uint64_t timestampDiff = unifiedTs > refTimestamp ? unifiedTs - refTimestamp : refTimestamp - unifiedTs;

	// We would delay the early arrived frame
	std::chrono::milliseconds delayMs(-lateMs);
	auto updateRefsPacketEarlyThresholdMs = -updateRefsStepPacketEarlyMs.count();

	//Log("-FrameDelayCalculator::OnFrame() | [ts:%lld,late:%lld,delay:%lld,timestampDiff:%lld]\n", unifiedTs, lateMs, delayMs.count(), timestampDiff);
	
	bool allEarly = false;
	//If we detect a clock difference between the timestamps
	if (refTimestamp && timestampDiff > MaxClockDesync)
	{
		//Reset calculator, we keep the refTimestamp so it is not set to the stream that has the jump
		Warning("-FrameDelayCalculator::OnFrame() | Timestamp clock jump detected [diff:%llu]\n",timestampDiff);
		allEarlyStartTimeMs.reset();
		frameArrivalInfo.erase(streamIdentifier);
		refTime = 0ms;
		return 0ms;
	}
	//If we were reseted because of a ts jump
	else if (!refTime.count())
	{
		//Store valid timestamp after reset
		refTime = now;
		refTimestamp = unifiedTs;
	}

    if (lateMs > updateRefsPacketLateThresholdMs)  // Packet late
	{
		refTime = now;
		refTimestamp = unifiedTs;
		
		delayMs = 0ms;
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
			
	//Log("-FrameDelayCalculator::OnFrame() | [delay:%lld,allEarly:%lld,allEarlyStartTime:%lld]\n", delayMs.count(), allEarly, allEarlyStartTimeMs.value_or(0ms).count());

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
	
	return std::max(delayMs, 0ms);
}

int64_t FrameDelayCalculator::GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs) const
{	
	constexpr int64_t MsToTimestampFactor = UnifiedClockRate / 1000;
	auto scheduledMs = (int64_t(unifiedTs) - int64_t(refTimestamp)) / MsToTimestampFactor + refTime.count();
	auto actualTimeMs = now.count();

	return actualTimeMs - scheduledMs;
}