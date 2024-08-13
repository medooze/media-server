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
		
		auto& checker = checkers[type];
		
		auto [result, rts] = checker.Check(time, ts, clk);
		ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
		ASSERT_EQ(ts, rts);
		ASSERT_EQ(0, checker.GetTimestampOffset());
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
		
		auto& checker = checkers[type];
		
		if (counter == 20)
		{
			auto [result, rts] = checker.Check(time, ts + 1000000, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
			ASSERT_EQ(2198852608, rts);
		}
		else
		{
			auto [result, rts] = checker.Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
			ASSERT_EQ(ts, rts);
		}
		
		ASSERT_EQ(0, checker.GetTimestampOffset());
		
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
		
		auto& checker = checkers[type];
		
		auto counter = counters[type];
		if (counter == 0)
		{
			ts = 1000000;
			auto [result, rts] = checker.Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
			
			EXPECT_EQ(0, checker.GetTimestampOffset());
		}
		else if (counter > 0 && counter < (1 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			// As first time stamp was used a reference, the following few frames would be regarded as invalid
			auto [result, rts] = checker.Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
			
			ASSERT_EQ(0, checker.GetTimestampOffset());
		}
		else if (counter == (1 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			auto [result, rts] = checker.Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Reset, result);
			
			ASSERT_EQ(type == MediaFrame::Type::Audio ? -2197841888 : -4121853071 , checker.GetTimestampOffset());
		}
		else
		{
			// After the continous invalid frames reach a threshold, the checking would be reset.
			auto [result, rts] = checker.Check(time, ts, clk);
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
			
			ASSERT_EQ(type == MediaFrame::Type::Audio ? -2197841888 : -4121853071 , checker.GetTimestampOffset());
		}
		
		counters[type]++;
	}
}

TEST(TestTimestampChecker, TimeStampWrong)
{
	std::unordered_map<MediaFrame::Type, TimestampChecker> checkers;
	std::unordered_map<MediaFrame::Type, uint32_t> counters;
	
	std::unordered_map<MediaFrame::Type, uint64_t> lastRts;
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts << " counter: " << counters[type];
		SCOPED_TRACE(ss.str());
		
		auto& checker = checkers[type];
		
		auto counter = counters[type];
		
		// Make a block of frames timestamp wrong and test it can be restored.
		
		if (counter >= 20 && counter <= 40)
		{
			ts = ts + 1000000;
		}
		
		auto [result, rts] = checker.Check(time, ts, clk);
		
		if (counter < 20)
		{
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
			ASSERT_EQ(0, checker.GetTimestampOffset());
		}
		else if (counter >= 20 && counter < (20 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{	
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
			
			ASSERT_EQ(0, checker.GetTimestampOffset());
		}
		else if (counter == (20 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimestampChecker::CheckResult::Reset, result);
			ASSERT_EQ(type == MediaFrame::Type::Audio ? -998032 : -999748, checker.GetTimestampOffset());
		}
		else if (counter < 41)
		{
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
			ASSERT_EQ(type == MediaFrame::Type::Audio ? -998032 : -999748, checker.GetTimestampOffset());
		}
		else if (counter >= 41 && counter < (41 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimestampChecker::CheckResult::Invalid, result);
			ASSERT_EQ(type == MediaFrame::Type::Audio ? -998032 : -999748, checker.GetTimestampOffset());
		}
		else if (counter == (41 + TimestampChecker::DefaultMaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimestampChecker::CheckResult::Reset, result);
			ASSERT_EQ(type == MediaFrame::Type::Audio ? -4176 : -396, checker.GetTimestampOffset());
		}
		else
		{	
			ASSERT_EQ(TimestampChecker::CheckResult::Valid, result);
			ASSERT_EQ(type == MediaFrame::Type::Audio ? -4176 : -396, checker.GetTimestampOffset());
		}
		
		// The timestamp should be mostly incremental with a resonable value.
		// We don't check incremental here as the calculated one might be not extactly same as the
		// one recieved from ingest due to recieving time delay
		auto maxDiff = clk * 0.3;
		if (lastRts.find(type) != lastRts.end())
		{
			ASSERT_LE(fabs(rts - lastRts[type]), maxDiff);
		}
		
		lastRts[type] = rts;
		
		counters[type]++;
	}
}