#include "FrameDelayCalculator.h"
#include <iostream>
#include <chrono>
#include "log.h"

using namespace std::chrono;

namespace
{

static constexpr uint64_t UnifiedClockRate = 90 * 1000;
static constexpr uint64_t MaxClockDesync = 100 * UnifiedClockRate; //100s
}

FrameDelayCalculator::FrameDelayCalculator(int aUpdateRefsPacketLateThresholdMs, 
					std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs, TimeService& timeService) :
	TimeServiceWrapper<FrameDelayCalculator>(timeService),
	updateRefsPacketLateThresholdMs(aUpdateRefsPacketLateThresholdMs),
	updateRefsStepPacketEarlyMs(aUpdateRefsStepPacketEarlyMs)
{
}

std::chrono::milliseconds FrameDelayCalculator::OnFrame(uint64_t streamIdentifier, std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate)
{	
	//Log("-FrameDelayCalculator::OnFrame() | [streamIdentifier:%lld,now:%lld,ts:%lld,clockRate:%lld,refTime:%lld,refTimestamp:%lld]\n", streamIdentifier, now.count(), ts, clockRate, refTime, refTimestamp);
	
	if (ts == 0) return 0ms;
	
	auto unifiedTs = ConvertTimestampClockRate(ts, clockRate, UnifiedClockRate);
	
	auto [refTime, refTimestamp] = reference.Get();
	
	if (state == State::Reset)
	{
		reference.Set(now, unifiedTs);
		
		state = State::Running;
		return 0ms;
	}
			
	// Note the lateMs could be negative when the frame arrives earlier than scheduled	
	auto lateMs = GetFrameArrivalDelayMs(now, unifiedTs, refTime, refTimestamp);
	
	//Calculate the timestamp diff to ensure we are using same 
	uint64_t timestampDiff = unifiedTs > refTimestamp ? unifiedTs - refTimestamp : refTimestamp - unifiedTs;

	// We would delay the early arrived frame
	std::chrono::milliseconds delayMs(-lateMs);
	auto updateRefsPacketEarlyThresholdMs = -updateRefsStepPacketEarlyMs.count();

	//Log("-FrameDelayCalculator::OnFrame() | [ts:%lld,late:%lld,delay:%lld,timestampDiff:%lld]\n", unifiedTs, lateMs, delayMs.count(), timestampDiff);

	//If we detect a clock difference between the timestamps
	if (timestampDiff > MaxClockDesync)
	{
		//Reset calculator, we keep the refTimestamp so it is not set to the stream that has the jump
		Warning("-FrameDelayCalculator::OnFrame() | Timestamp clock jump detected [diff:%llu]\n",timestampDiff);
		state = State::Reset;
	}

	bool early = false;
	if (state == State::Reset || lateMs > updateRefsPacketLateThresholdMs)  // Packet late
	{
		reference.Set(now, unifiedTs);
		
		delayMs = 0ms;
	}
	else if (lateMs < updateRefsPacketEarlyThresholdMs)  // Packet early
	{
		early = true;
	}
	
	// Asynchronously check if we can reduce latency if all frames comes early
	AsyncSafe([early, now, unifiedTs, refTime, refTimestamp, 
				streamIdentifier, updateRefsPacketEarlyThresholdMs, state = state](auto self, std::chrono::milliseconds) {
		
		if (state == State::Reset)
		{
			self->frameArrivalInfo.erase(streamIdentifier);
			self->allEarlyStartTimeMs.reset();
			return;
		}
		
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
				auto frameLateMs = GetFrameArrivalDelayMs(time, timestamp, refTime, refTimestamp);
				
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
				self->reference.Set(refTime - self->updateRefsStepPacketEarlyMs, refTimestamp);

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
	
	return std::max(delayMs, 0ms);
}

int64_t FrameDelayCalculator::GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs, std::chrono::milliseconds refTime, uint64_t refTimestamp)
{	
	constexpr int64_t MsToTimestampFactor = UnifiedClockRate / 1000;
	auto scheduledMs = (int64_t(unifiedTs) - int64_t(refTimestamp)) / MsToTimestampFactor + refTime.count();
	auto actualTimeMs = now.count();

	return actualTimeMs - scheduledMs;
}

std::pair<std::chrono::milliseconds, uint64_t> FrameDelayCalculator::Reference::Get()
{
	static_assert(sizeof(ReferenceField) == 16);
	static_assert(sizeof(ReferenceField::value) == 16);
	static_assert(sizeof(ReferenceField::content) == 16);
	
	ReferenceField currentRef;
	currentRef.value = __sync_fetch_and_add(&field, 0);
	
	return { std::chrono::milliseconds(currentRef.content.refTime), currentRef.content.refTimestamp };
}

void FrameDelayCalculator::Reference::Set(std::chrono::milliseconds refTime, uint64_t refTimestamp)
{
	ReferenceField newRef = {{refTime.count(), refTimestamp}};
	
	SyncWriteUint128(&field, newRef.value);
}
