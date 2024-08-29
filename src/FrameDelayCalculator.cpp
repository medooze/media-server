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

}

FrameDelayCalculator::FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs, TimeService& timeService) :
	updateRefsPacketLateThresholdMs(aUpdateRefsPacketLateThresholdMs),
	updateRefsStepPacketEarlyMs(aUpdateRefsStepPacketEarlyMs),
	timeService(timeService)
{
}

std::chrono::milliseconds FrameDelayCalculator::OnFrame(uint64_t streamIdentifier, std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate)
{	
	//Log("S: %lld, now: %lld, ts: %lld, clk: %lld\n", streamIdentifier, now.count(), ts, clockRate);
	
	if (ts == 0) return std::chrono::milliseconds(0);
	
	auto unifiedTs = ConvertTimestampClockRate(ts, clockRate, UnifiedClockRate);
	
	auto [refTime, refTimestamp] = reference.Get();
	
	if (refTime == 0)
	{
		reference.Set(now.count(), unifiedTs);
		
		return std::chrono::milliseconds(0);
	}
			
	// Note the lateMs could be negative when the frame arrives earlier than scheduled	
	auto lateMs = GetFrameArrivalDelayMs(now, unifiedTs,
				std::chrono::milliseconds(refTime), refTimestamp);
	
	// We would delay the early arrived frame
	std::chrono::milliseconds delayMs(-lateMs);
	auto updateRefsPacketEarlyThresholdMs = -updateRefsStepPacketEarlyMs.count();
	
	bool early = false;
	if (lateMs > updateRefsPacketLateThresholdMs)  // Packet late
	{
		reference.Set(now.count(), unifiedTs);
		
		delayMs = std::chrono::milliseconds(0);
	}
	else if (lateMs < updateRefsPacketEarlyThresholdMs)  // Packet early
	{
		early = true;
	}
	
	// Asynchronously check if we can reduce latency if all frames comes early
	timeService.Async([selfWeak = weak_from_this(), early, now, unifiedTs, refTime, refTimestamp, 
				streamIdentifier, updateRefsPacketEarlyThresholdMs](std::chrono::milliseconds) {
		
		auto self = selfWeak.lock();
		if (!self) return;
		
		self->frameArrivalInfo[streamIdentifier] = {now, unifiedTs};
		
		if (!early)
		{
			self->allEarlyStartTimeMs.reset();
			return;
		}
		
		// Loop to see if all the streams have arrived earlier
		bool allEarly = std::all_of(self->frameArrivalInfo.begin(), self->frameArrivalInfo.end(), 
			[&](const auto& info) {
				if (info.first == streamIdentifier) return true;
							
				auto [time, timestamp] = info.second;
				auto frameLateMs = GetFrameArrivalDelayMs(time, timestamp, std::chrono::milliseconds(refTime), refTimestamp);
				
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
				self->reference.Set(refTime - self->updateRefsStepPacketEarlyMs.count(), refTimestamp);

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

std::pair<int64_t, uint64_t> FrameDelayCalculator::Reference::Get()
{
	static_assert(sizeof(ReferenceField) == 16);
	static_assert(sizeof(ReferenceField::value) == 16);
	static_assert(sizeof(ReferenceField::content) == 16);
	
	ReferenceField currentRef;
	currentRef.value = __sync_fetch_and_add(&field, 0);
	
	return { currentRef.content.refTime, currentRef.content.refTimestamp };
}

void FrameDelayCalculator::Reference::Set(int64_t refTime, uint64_t refTimestamp)
{
	ReferenceField newRef;
	newRef.content = {refTime, refTimestamp};
	
	SyncWriteUint128(&field, newRef.value);
}
		