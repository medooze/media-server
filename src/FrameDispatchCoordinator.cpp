#include "FrameDispatchCoordinator.h"
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

FrameDispatchCoordinator::FrameDispatchCoordinator(int aUpdateRefsPacketEarlyThresholdMs,
							int aUpdateRefsPacketLateThresholdMs, 
							std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs) :
	updateRefsPacketEarlyThresholdMs(aUpdateRefsPacketEarlyThresholdMs),
	updateRefsPacketLateThresholdMs(aUpdateRefsPacketLateThresholdMs),
	updateRefsStepPacketEarlyMs(aUpdateRefsStepPacketEarlyMs)
{
}

void FrameDispatchCoordinator::OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge)
{	
	auto unifiedTs = convertTimestampClockRate(ts, clockRate, UnifiedClockRate);
	
	std::lock_guard<std::mutex> lock(mutex);
		
	if (!initialized)
	{
		refTime = now;
		refTimestamp = unifiedTs;
		
		listenerBridge.SetDelayMs(std::chrono::milliseconds(0));
		
		initialized = true;			
		return;
	}
			
	// Note the lateMs could be negative when the frame arrives earlier than scheduled	
	auto lateMs = GetFrameArrivalDelayMs(now, unifiedTs);
	
	// We would delay the early arrived frame
	std::chrono::milliseconds delayMs(-lateMs);
	
	if (lateMs > updateRefsPacketLateThresholdMs)  // Packet late
	{
		refTime = now;
		refTimestamp = unifiedTs;
		
		// Stop reducing latency as a packet becomes late
		reducingLatency = false;
		delayMs = std::chrono::milliseconds(0);
	}
	else if (!reducingLatency && lateMs < updateRefsPacketEarlyThresholdMs)  // Packet early
	{
		// Loop to see if all the streams have arrived earlier
		reducingLatency = std::all_of(frameArrivalInfo.begin(), frameArrivalInfo.end(), 
			[&](const auto& info) {
				if (info.first == &listenerBridge) return true;
							
				auto [time, timestamp] = info.second;
				auto frameLateMs = GetFrameArrivalDelayMs(time, timestamp);
				
				return frameLateMs < updateRefsPacketEarlyThresholdMs;
			});
	}
	else if (reducingLatency && lateMs > -updateRefsStepPacketEarlyMs.count())
	{
		// Stop reducing latency otherwise the frame would be regarded as late if we
		// do a further step of latency reduction.
		reducingLatency = false;
	}

	frameArrivalInfo[&listenerBridge] = {now, unifiedTs};
				
	if (reducingLatency)
	{
		// Make reference time earlier for same time stamp, which means
		// frames will be dispatched ealier.
		refTime -= updateRefsStepPacketEarlyMs;
		
		// Reduce the delay
		delayMs -= updateRefsStepPacketEarlyMs;
	}
	
	listenerBridge.SetDelayMs(std::max(delayMs, std::chrono::milliseconds(0)));
}

int64_t FrameDispatchCoordinator::GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs) const
{	
	constexpr int64_t MsToTimestampFactor = UnifiedClockRate / 1000;
	auto scheduledMs = (int64_t(unifiedTs) - int64_t(refTimestamp)) / MsToTimestampFactor + refTime.count();
	auto actualTimeMs = now.count();

	return actualTimeMs - scheduledMs;
}