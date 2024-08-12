#include "TestCommon.h"
#include "TimestampChecker.h"
#include "data/FramesArrivalInfo.h"

TEST(TestTimestampChecker, NormalStream)
{
	std::unordered_map<MediaFrame::Type, TimestampChecker> checkers;
	
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{	
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts;
		SCOPED_TRACE(ss.str());
		
		auto [result, rts] = checkers[type].Check(time, ts, clk);
		ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
	}
}

TEST(TestTimestampChecker, TimestampSpike)
{
	std::unordered_map<MediaFrame::Type, TimestampChecker> checkers;
	
	uint32_t counter = 0;
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts << " counter: " << counter;
		SCOPED_TRACE(ss.str());
		
		if (counter == 20)
		{
			ts = ts + 1000000;
			
			auto [result, rts] = checkers[type].Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
		}
		else
		{
			auto [result, rts] = checkers[type].Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
		}
		
		counter++;
	}
}

TEST(TestTimestampChecker, FirstTimeStampWrong)
{
	std::unordered_map<MediaFrame::Type, TimestampChecker> checkers;
	std::unordered_map<MediaFrame::Type, uint32_t> counters;
	
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts << " counter: " << counters[type];
		SCOPED_TRACE(ss.str());
		
		auto counter = counters[type];
		if (counter == 0)
		{
			ts = 1000000;
			auto [result, rts] = checkers[type].Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
		}
		else if (counter > 0 && counter < (1 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			// As firt time stamp was used a reference, the following few frames would be regarded as invalid
			auto [result, rts] = checkers[type].Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
		}
		else if (counter == (1 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			auto [result, rts] = checkers[type].Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Reset, result);
		}
		else
		{
			// After the continous invalid frames reach a threshold, the checking would be reset.
			auto [result, rts] = checkers[type].Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
		}
		
		counters[type]++;
	}
}

TEST(TestTimestampChecker, TimeStampWrong)
{
	std::unordered_map<MediaFrame::Type, TimestampChecker> checkers;
	std::unordered_map<MediaFrame::Type, uint32_t> counters;
	
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts << " counter: " << counters[type];
		SCOPED_TRACE(ss.str());
		
		auto counter = counters[type];
		
		// Make a block of frames timestamp wrong and test it can be restored.
		
		if (counter >= 20 && counter <= 40)
		{
			ts = ts + 1000000;
		}
		
		auto [result, rts] = checkers[type].Check(time, ts, clk);
		
		if (counter >= 20 && counter < (20 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{	
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
		}
		else if (counter >= 41 && counter < (41 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
		}
		else if (counter == (20 + TimestampChecker::DefaultMaxContinousInvalidFrames) || 
			counter == (41 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimestampChecker::CheckResult::Reset, result);
		}
		else
		{	
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
		}
		
		counters[type]++;
	}
}