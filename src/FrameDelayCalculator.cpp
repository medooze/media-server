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

/**
 * Atomatically write a 128bit integer to an address
 */
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

static constexpr uint64_t UnifiedClockRate = 90 * 1000;

}

FrameDelayCalculator::FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs, TimeService& timeService) :
	updateRefsPacketLateThresholdMs(aUpdateRefsPacketLateThresholdMs),
	updateRefsStepPacketEarlyMs(aUpdateRefsStepPacketEarlyMs),
	timeService(timeService)
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
	currentRef.value = __sync_fetch_and_add(&reference.value, 0);
	
	if (currentRef.value == 0)
	{
		currentRef.content = { now.count(), unifiedTs };
		SyncWriteUint128(&reference.value, currentRef.value);
		
		return std::chrono::milliseconds(0);
	}
			
	// Note the lateMs could be negative when the frame arrives earlier than scheduled	
	auto lateMs = GetFrameArrivalDelayMs(now, unifiedTs,
				std::chrono::milliseconds(currentRef.content.refTime), currentRef.content.refTimestamp);
	
	// We would delay the early arrived frame
	std::chrono::milliseconds delayMs(-lateMs);
	auto updateRefsPacketEarlyThresholdMs = -updateRefsStepPacketEarlyMs.count();
	
	bool early = false;
	if (lateMs > updateRefsPacketLateThresholdMs)  // Packet late
	{
		currentRef.content = { now.count(), unifiedTs };
		SyncWriteUint128(&reference.value, currentRef.value);
		
		delayMs = std::chrono::milliseconds(0);
	}
	else if (lateMs < updateRefsPacketEarlyThresholdMs)  // Packet early
	{
		early = true;
	}
	
	// Asynchronously check if we can reduce latency if all frames comes early
	timeService.Async([selfWeak = weak_from_this(), early, now, unifiedTs, currentRef, 
				streamIdentifier, updateRefsPacketEarlyThresholdMs](std::chrono::milliseconds) {
		
		auto self = selfWeak.lock();
		if (!self) return;
		
		self->frameArrivalInfo[streamIdentifier] = {now, unifiedTs};
		
		if (!early)
		{
			self->allEarlyStartTimeMs.reset();
			return;
		}
		
		auto ref = currentRef;
		
		// Loop to see if all the streams have arrived earlier
		bool allEarly = std::all_of(self->frameArrivalInfo.begin(), self->frameArrivalInfo.end(), 
			[&](const auto& info) {
				if (info.first == streamIdentifier) return true;
							
				auto [time, timestamp] = info.second;
				auto frameLateMs = GetFrameArrivalDelayMs(time, timestamp, std::chrono::milliseconds(ref.content.refTime),
									ref.content.refTimestamp);
				
				return frameLateMs < updateRefsPacketEarlyThresholdMs;
			});
			
		if (allEarly)
		{
			if (!self->allEarlyStartTimeMs.has_value())
				self->allEarlyStartTimeMs = now;
				
			// If all stream becomes ealier for a while (> 2s), we reduce the latency
			if ((now - *self->allEarlyStartTimeMs) > 2000ms)
			{
				// Make reference time earlier for same time stamp, which means
				// frames will be dispatched ealier.
				ref.content.refTime -= self->updateRefsStepPacketEarlyMs.count();
				SyncWriteUint128(&self->reference.value, ref.value);

				// Restart the latency reduction process to have a max reduction rate
				// at 20ms per second as we don't expect the latency would be too large,
				// which normall would be below 1 second.
				self->allEarlyStartTimeMs.reset();
			}
		}
		else
		{
			self->allEarlyStartTimeMs.reset();
		}
	});
	
	return std::max(delayMs, std::chrono::milliseconds(0));
}

int64_t FrameDelayCalculator::GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs, std::chrono::milliseconds refTime, uint64_t refTimestamp)
{	
	constexpr int64_t MsToTimestampFactor = UnifiedClockRate / 1000;
	auto scheduledMs = (int64_t(unifiedTs) - int64_t(refTimestamp)) / MsToTimestampFactor + refTime.count();
	auto actualTimeMs = now.count();

	return actualTimeMs - scheduledMs;
}