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
 * If the time stamp is regarded as invalid, a rectified timestamp would be calculated as per
 * the receiving time and current timestamp reference, including the offset.
 * 
 * If the time and timestamps pair are continously regarded as invalid for a certain frames,
 * the time checker will be reset. When there is a reset, an offset will be calculated to ensure
 * the rectified timestamp to be incremental and sensible to the receiving time. After reset,
 * the rectified timestamp will be calculated basing on the actual received timestamp and offset.
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
	
	/**
	 * Check the timestamp
	 * 
	 * @param recvTimeMs The receiving time, in milliseconds
	 * @param timestamp  The timestamp to be checked.
	 * @param clock      The timestamp clock 
	 * 
	 * @return  A pair of check result and rectified timestamp. Caller should use the rectified
	 *	    timestamp to ensure the timestamp to be incremental and sensible.
	 */
	std::pair<CheckResult, uint64_t> Check(uint64_t recvTimeMs, uint64_t timestamp, uint32_t clock)
	{
		if (clock == 0) return { CheckResult::Invalid, 0};
		
		// Check if it is just started
		if (!initialised)
		{
			lastTimeStamp = timestamp;
			lastRecvTime = recvTimeMs;
			
			initialised = true;
			
			return {CheckResult::Valid, timestamp};
		}
		
		auto durationOnTs = GetAbsDiff(timestamp, lastTimeStamp) * 1000 / clock;
		auto durationOnRecv = GetAbsDiff(recvTimeMs, lastRecvTime);
		
		bool sameOrder = (recvTimeMs >= lastRecvTime && timestamp >= lastTimeStamp) || 
				 (recvTimeMs <= lastRecvTime && timestamp <= lastTimeStamp);
		
		// The difference between expected duration as per timestamp and actual duration since last frame
		auto diff = sameOrder ? GetAbsDiff(durationOnTs, durationOnRecv) : (durationOnTs + durationOnRecv);
		
		auto result = CheckResult::Valid;
		int64_t rectifiedTs = timestamp;
		
		bool valid = diff < maxDurationDiffMs;
		if (valid)
		{
			lastTimeStamp = timestamp;
			lastRecvTime = recvTimeMs;
			
			numContinousInvalidFrames = 0;
			
			rectifiedTs = timestamp + offset;
			
			result = CheckResult::Valid;
		}
		else
		{
			numContinousInvalidFrames++;
			
			rectifiedTs = (int64_t(recvTimeMs) - int64_t(lastRecvTime))* clock / 1000 + lastTimeStamp + offset;

			// If there are continous invalid frames, it could be due to reference wrong. Reset to current.
			if (numContinousInvalidFrames > maxContinousInvalidFrames)
			{
				lastTimeStamp = timestamp;
				lastRecvTime = recvTimeMs;
				numContinousInvalidFrames = 0;
				
				offset = rectifiedTs - timestamp;
				
				result = CheckResult::Reset;
			}
			else
			{
				result = CheckResult::Invalid;
			}
		}
		
		return { result, uint64_t(rectifiedTs) };
	}
	
	inline int64_t GetTimestampOffset() const
	{
		return offset;
	}
	
	inline static constexpr uint64_t GetAbsDiff(uint64_t lhs, uint64_t rhs)
	{
		return lhs > rhs ? (lhs - rhs) : (rhs - lhs);
	}
	
private:
	uint32_t maxDurationDiffMs = 0;
	uint32_t maxContinousInvalidFrames = 0;

	uint64_t lastTimeStamp = 0;
	uint64_t lastRecvTime = 0;
	
	uint32_t numContinousInvalidFrames = 0;
	
	int64_t offset = 0;
	
	bool initialised = false;
};



#endif