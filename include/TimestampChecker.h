#ifndef TIMESTAMPCHECKER_H
#define TIMESTAMPCHECKER_H

#include <media.h>
#include <acumulator.h>

/**
 * Check timestamp as per past timestamps and receiving (frame arrival) time to determine
 * whether the timestamp is valid. 
 * 
 * It assumes the receiving time is always correct, as it is set on the server. The duration
 * calculated from timestamp since last frame will be compared with the actual receiving time
 * gap. If the difference between the two is too big, it would be regarded as invalid. 
 * 
 * At starting/resetting, the first frame is always regarded as valid. Invalid frames are not
 * used for reference.
 * 
 * If the time and timestamps pair are continously regarded as invalid for a certain frames,
 * the time checker will be reset.
 */
class TimestampChecker
{
public:
	static constexpr uint32_t DefaultMaxDurationDiffMs = 3000;
	static constexpr uint32_t DefaultMaxContinousInvalidFrames = 5;
	
	enum class CheckResult
	{
		Valid,
		Invalid,
		Reset
	};
	
	TimestampChecker(uint32_t maxDurationDiffMs = DefaultMaxDurationDiffMs, 
			uint32_t maxContinousInvalidFrames = DefaultMaxContinousInvalidFrames) :
		maxDurationDiffMs(maxDurationDiffMs),
		maxContinousInvalidFrames(maxContinousInvalidFrames)
	{
	}
	
	CheckResult Check(uint64_t recvTimeMs, uint64_t timestamp, uint32_t clock)
	{
		if (clock == 0) return CheckResult::Invalid;
		
		// Check if it is just started
		if (lastRecvTime == 0 && lastTimeStamp == 0)
		{
			lastTimeStamp = timestamp;
			lastRecvTime = recvTimeMs;
			return CheckResult::Valid;
		}
		
		auto durationOnTs = GetDiff(timestamp, lastTimeStamp) * 1000 / clock;
		auto durationOnRecv = GetDiff(recvTimeMs, lastRecvTime);
		
		bool sameOrder = (recvTimeMs >= lastRecvTime && timestamp >= lastTimeStamp) || 
				 (recvTimeMs <= lastRecvTime && timestamp <= lastTimeStamp);
		
		// The difference between expected duration as per timestamp and actual duration since last frame
		auto diff = sameOrder ? GetDiff(durationOnTs, durationOnRecv) : (durationOnTs + durationOnRecv);
		
		bool valid = diff < maxDurationDiffMs;
		if (valid)
		{
			lastTimeStamp = timestamp;
			lastRecvTime = recvTimeMs;
			
			numContinousInvalidFrames = 0;
		}
		else
		{
			numContinousInvalidFrames++;
		}
		
		// If there are continous invalid frames, it could be due to reference wrong. Reset to current.
		if (numContinousInvalidFrames > maxContinousInvalidFrames)
		{
			lastTimeStamp = timestamp;
			lastRecvTime = recvTimeMs;
			numContinousInvalidFrames = 0;
			
			return CheckResult::Reset;
		}

		return valid ? CheckResult::Valid : CheckResult::Invalid;
	}
	
	inline static constexpr uint64_t GetDiff(uint64_t lhs, uint64_t rhs)
	{
		return lhs > rhs ? (lhs - rhs) : (rhs - lhs);
	}
	
private:
	uint32_t maxDurationDiffMs = 0;
	uint32_t maxContinousInvalidFrames = 0;

	uint64_t lastTimeStamp = 0;
	uint64_t lastRecvTime = 0;
	
	uint32_t numContinousInvalidFrames = 0;
};



#endif