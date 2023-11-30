#include "TestCommon.h"
#include "TimeStampChecker.h"
#include "data/FramesArrivalInfo.h"

#include "video.h"
#include "audio.h"


std::unique_ptr<MediaFrame> CreateTestFrame(MediaFrame::Type type, uint64_t time, uint64_t timestamp, uint32_t clock)
{
	std::unique_ptr<MediaFrame> frame;
	
	if (type == MediaFrame::Video)
	{
		frame = std::make_unique<VideoFrame>(VideoCodec::H264);
	}
	else
	{
		frame = std::make_unique<AudioFrame>(AudioCodec::AAC);
	}
	
	frame->SetTime(time);
	frame->SetTimestamp(timestamp);
	frame->SetClockRate(clock);
	frame->SetLength(100);
	frame->AddRtpPacket(0, 100);
	
	return frame;
}

TEST(TestTimeStampChecker, NormalStream)
{
	std::unordered_map<MediaFrame::Type, TimeStampChecker> checkers;
	
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{	
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts;
		SCOPED_TRACE(ss.str());
		
		auto frame = CreateTestFrame(type, time, ts, clk);
		
		ASSERT_EQ(TimeStampChecker::CheckResult::Valid, checkers[type].Check(*frame));
	}
}

TEST(TestTimeStampChecker, TimestampSpike)
{
	std::unordered_map<MediaFrame::Type, TimeStampChecker> checkers;
	
	uint32_t counter = 0;
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts << " counter: " << counter;
		SCOPED_TRACE(ss.str());
		
		auto frame = CreateTestFrame(type, time, ts, clk);
		
		if (counter == 20)
		{
			frame->SetTimestamp(frame->GetTimeStamp() + 1000000);
			ASSERT_EQ(TimeStampChecker::CheckResult::Invalid, checkers[type].Check(*frame));
		}
		else
		{
			ASSERT_EQ(TimeStampChecker::CheckResult::Valid, checkers[type].Check(*frame));
		}
		
		counter++;
	}
}

TEST(TestTimeStampChecker, FirstTimeStampWrong)
{
	std::unordered_map<MediaFrame::Type, TimeStampChecker> checkers;
	std::unordered_map<MediaFrame::Type, uint32_t> counters;
	
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts << " counter: " << counters[type];
		SCOPED_TRACE(ss.str());
		
		auto frame = CreateTestFrame(type, time, ts, clk);
		
		auto counter = counters[type];
		if (counter == 0)
		{
			frame->SetTimestamp(frame->GetTimeStamp() + 1000000);
			ASSERT_EQ(TimeStampChecker::CheckResult::Valid, checkers[type].Check(*frame));
		}
		else if (counter > 0 && counter < (1 + TimeStampChecker::MaxContinousInvalidFrames))
		{
			// As firt time stamp was used a reference, the following few frames would be regarded as invalid
			ASSERT_EQ(TimeStampChecker::CheckResult::Invalid, checkers[type].Check(*frame));
		}
		else if (counter == (1 + TimeStampChecker::MaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimeStampChecker::CheckResult::Reset, checkers[type].Check(*frame));
		}
		else
		{
			// After the continous invalid frames reach a threshold, the checking would be reset.
			ASSERT_EQ(TimeStampChecker::CheckResult::Valid, checkers[type].Check(*frame));
		}
		
		counters[type]++;
	}
}

TEST(TestTimeStampChecker, TimeStampWrong)
{
	std::unordered_map<MediaFrame::Type, TimeStampChecker> checkers;
	std::unordered_map<MediaFrame::Type, uint32_t> counters;
	
	for (auto [type, time, ts, clk] : TestData::FramesArrivalInfo)
	{
		std::stringstream ss;
		ss << "type: " << type << " time: " << time << " ts: " << ts << " counter: " << counters[type];
		SCOPED_TRACE(ss.str());
		
		auto frame = CreateTestFrame(type, time, ts, clk);
		
		auto counter = counters[type];
		
		// Make a block of frames timestamp wrong and test it can be restored.
		
		if (counter >= 20 && counter <= 40)
		{
			frame->SetTimestamp(frame->GetTimeStamp() + 1000000);
		}
		
		if (counter >= 20 && counter < (20 + TimeStampChecker::MaxContinousInvalidFrames))
		{	
			ASSERT_EQ(TimeStampChecker::CheckResult::Invalid, checkers[type].Check(*frame));
		}
		else if (counter >= 41 && counter < (41 + TimeStampChecker::MaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimeStampChecker::CheckResult::Invalid, checkers[type].Check(*frame));
		}
		else if (counter == (20 + TimeStampChecker::MaxContinousInvalidFrames) || 
			counter == (41 + TimeStampChecker::MaxContinousInvalidFrames))
		{
			ASSERT_EQ(TimeStampChecker::CheckResult::Reset, checkers[type].Check(*frame));
		}
		else
		{	
			ASSERT_EQ(TimeStampChecker::CheckResult::Valid, checkers[type].Check(*frame));
		}
		
		counters[type]++;
	}
}