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
static constexpr T ConvertTimestampClockRate(T ts, uint64_t originalRate, uint64_t targetRate)
{
	static_assert(sizeof(T) >= 8);
	return originalRate == targetRate ? ts : (ts * T(targetRate) / T(originalRate));
}

static constexpr uint64_t UnifiedClockRate = 90 * 1000;

void SyncWriteUint128(__uint128_t *dst, __uint128_t value)
{
    __uint128_t dstval = 0;
    __uint128_t olddst = 0;
    
    dstval = *dst;
    do
    {
        olddst = dstval;
        dstval = __sync_val_compare_and_swap(dst, dstval, value);
    }
    while(dstval != olddst);
}

}

FrameDelayCalculator::FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	updateRefsPacketLateThresholdMs(aUpdateRefsPacketLateThresholdMs),
	updateRefsStepPacketEarlyMs(aUpdateRefsStepPacketEarlyMs)
{
	static_assert(sizeof(Reference) == 16);
	static_assert(sizeof(reference.content) == 16);
}

std::chrono::milliseconds FrameDelayCalculator::OnFrame(uint64_t streamIdentifier, std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate)
{	
	//Log("S: %lld, now: %lld, ts: %lld, clk: %lld\n", streamIdentifier, now.count(), ts, clockRate);
	
	if (ts == 0) return std::chrono::milliseconds(0);
	
	auto unifiedTs = ConvertTimestampClockRate(ts, clockRate, UnifiedClockRate);
	
	Reference currentRef;
	currentRef.value = __sync_fetch_and_add((__uint128_t*)&reference, 0);
	
	if (!initialized)
	{
		currentRef.content = { now.count(), unifiedTs };
		SyncWriteUint128(&reference.value, currentRef.value);
		
		initialized = true;			
		return std::chrono::milliseconds(0);
	}
			
	// Note the lateMs could be negative when the frame arrives earlier than scheduled	
	auto lateMs = GetFrameArrivalDelayMs(now, unifiedTs,
				std::chrono::milliseconds(currentRef.content.refTime), currentRef.content.refTimestamp);
	
	// We would delay the early arrived frame
	std::chrono::milliseconds delayMs(-lateMs);
	auto updateRefsPacketEarlyThresholdMs = -updateRefsStepPacketEarlyMs.count();
	
	bool allEarly = false;
	if (lateMs > updateRefsPacketLateThresholdMs)  // Packet late
	{
		currentRef.content = { now.count(), unifiedTs };
		SyncWriteUint128(&reference.value, currentRef.value);
		
		delayMs = std::chrono::milliseconds(0);
	}
	else if (lateMs < updateRefsPacketEarlyThresholdMs)  // Packet early
	{
		// Loop to see if all the streams have arrived earlier
		allEarly = std::all_of(frameArrivalInfo.begin(), frameArrivalInfo.end(), 
			[&](const auto& info) {
				if (info.first == streamIdentifier) return true;
							
				auto [time, timestamp] = info.second;
				auto frameLateMs = GetFrameArrivalDelayMs(time, timestamp, std::chrono::milliseconds(currentRef.content.refTime),
									currentRef.content.refTimestamp);
				
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
		currentRef.content.refTime -= updateRefsStepPacketEarlyMs.count();
		SyncWriteUint128(&reference.value, currentRef.value);
		
		// Reduce the delay
		delayMs -= updateRefsStepPacketEarlyMs;
		
		// Restart the latency reduction process to have a max reduction rate
		// at 20ms per second as we don't expect the latency would be too large,
		// which normall would be below 1 second.
		allEarlyStartTimeMs.reset();
	}
	
	return std::max(delayMs, std::chrono::milliseconds(0));
}

int64_t FrameDelayCalculator::GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs, std::chrono::milliseconds refTime, uint64_t refTimestamp)
{	
	constexpr int64_t MsToTimestampFactor = UnifiedClockRate / 1000;
	auto scheduledMs = (int64_t(unifiedTs) - int64_t(refTimestamp)) / MsToTimestampFactor + refTime.count();
	auto actualTimeMs = now.count();

	return actualTimeMs - scheduledMs;
}