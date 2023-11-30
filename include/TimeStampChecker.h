#ifndef TIMESTAMPCHECKER_H
#define TIMESTAMPCHECKER_H

#include <media.h>
#include <acumulator.h>

class TimeStampChecker
{
public:
	static constexpr uint32_t MaxDurationDiffMs = 3000;
	static constexpr uint32_t MaxContinousInvalidFrames = 5;
	
	enum class CheckResult
	{
		Valid,
		Invalid,
		Reset
	};
	
	CheckResult Check(const MediaFrame &frame)
	{
		// Check if it is just started
		if (lastRecvTime == 0 && lastTimeStamp == 0)
		{
			lastTimeStamp = frame.GetTimeStamp();
			lastRecvTime = frame.GetTime();
			return CheckResult::Valid;
		}
		
		auto durationOnTs = (frame.GetTimeStamp() - lastTimeStamp) * 1000 / frame.GetClockRate();
		auto durationOnRecv = frame.GetTime() - lastRecvTime;
		
		// The difference between expected duration as per timestamp and actual duration since last frame
		auto diff = int64_t(durationOnTs) - int64_t(durationOnRecv);
		
		bool valid = labs(diff) < MaxDurationDiffMs;
		if (valid)
		{
			lastTimeStamp = frame.GetTimeStamp();
			lastRecvTime = frame.GetTime();
			
			numContinousInvalidFrames = 0;
		}
		else
		{
			numContinousInvalidFrames++;
		}
		
		// If there are continous invalid frames, it could be due to reference wrong. Reset to restart.
		if (numContinousInvalidFrames > MaxContinousInvalidFrames)
		{
			lastRecvTime = 0;
			lastTimeStamp = 0;
			numContinousInvalidFrames = 0;
			
			return CheckResult::Reset;
		}

		return valid ? CheckResult::Valid : CheckResult::Invalid;
	}
	
private:
	uint64_t lastTimeStamp = 0;
	uint64_t lastRecvTime = 0;
	
	uint32_t numContinousInvalidFrames = 0;
};



#endif