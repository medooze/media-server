#include "SimulcastMediaFrameListener.h"
#include "TimeService.h"
#include "video.h"

#include <future>
#include <include/gtest/gtest.h>
#include <memory>

namespace {
static constexpr uint32_t ClockRate = 90 * 1000;
static constexpr uint64_t TimestampInterval = 2970;
static constexpr uint64_t FrameTimeIntervalMs = TimestampInterval * 1000 / ClockRate;
}

class TestFrameGenerator
{
public:
	TestFrameGenerator(uint32_t ssrc, uint32_t width, uint32_t height, uint64_t initialTime, uint64_t initialTimestamp) :
		ssrc(ssrc),
		width(width),
		height(height),
		nextFrameTime(initialTime),
		nextTimestamp(initialTimestamp)
	{
	}

	std::unique_ptr<VideoFrame> Generate(bool isIntra = false)
	{
		auto frame = std::make_unique<VideoFrame>(VideoCodec::Type::VP8);
		frame->SetSSRC(ssrc);
		frame->SetWidth(width);
		frame->SetHeight(height);
		frame->SetTimestamp(nextTimestamp);
		frame->SetTime(nextFrameTime);
		frame->SetIntra(isIntra);
		frame->SetLength(width * height);

		nextTimestamp += TimestampInterval;
		nextFrameTime += FrameTimeIntervalMs;

		return std::move(frame);
	}

private:
	uint32_t ssrc;
	uint32_t width;
	uint32_t height;

	uint64_t nextFrameTime;
	uint64_t nextTimestamp;
};

class TestTimeService : public TimeService
{
public:
	virtual const std::chrono::milliseconds GetNow() const override 
	{ 
		assert(false); // Not used
		return std::chrono::milliseconds(); 
	}
	virtual Timer::shared CreateTimer(std::function<void(std::chrono::milliseconds)> callback) override
	{ 
		assert(false); // Not used
		return nullptr; 
	}
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, std::function<void(std::chrono::milliseconds)> timeout) override
	{
		assert(false); // Not used
		return nullptr;
	}
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, 
										const std::chrono::milliseconds& repeat, 
										std::function<void(std::chrono::milliseconds)> timeout) override
	{
		assert(false); // Not used
		return nullptr;
	};

	virtual std::future<void> Async(std::function<void(std::chrono::milliseconds)> func) override
	{		
		return std::async(std::launch::deferred, [&](){ func(std::chrono::milliseconds()); });
	}
};

class TestMediaFrameListner: public MediaFrame::Listener
{
public:
	virtual void onMediaFrame(const MediaFrame &frame) override
	{
		assert(false); // Not used
	}

	virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame) override
	{
		forwardedFrames.emplace_back(dynamic_cast<const VideoFrame&>(frame).GetWidth(), frame.GetTimeStamp());
	}

	std::vector<std::pair<uint32_t, uint64_t>> forwardedFrames;
};

class TestSimulcastMediaFrameListener : public ::testing::Test
{
public:
	TestSimulcastMediaFrameListener() : 
		timerService(), 
		listener(timerService, 0, 3), 
		mediaFrameListener(new TestMediaFrameListner) 
	{
		listener.AddMediaListener(mediaFrameListener);
	}

	void PushFrame(std::unique_ptr<VideoFrame> frame)
	{
		listener.onMediaFrame(frame->GetSSRC(), *frame);
	}

	std::vector<std::pair<uint32_t, uint64_t>> PopForwardedFrames()
	{
		return std::move(mediaFrameListener->forwardedFrames);
	}

protected:
	TestTimeService timerService;
	std::shared_ptr<TestMediaFrameListner> mediaFrameListener;
	SimulcastMediaFrameListener listener;
};


TEST_F(TestSimulcastMediaFrameListener, LayerSelection) 
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1000, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1000, 30000);
	
	PushFrame(low.Generate(true));
	PushFrame(mid.Generate(true));
	PushFrame(high.Generate(true));

	for (size_t i = 0; i < 14; i++)
	{
		PushFrame(low.Generate());
		PushFrame(mid.Generate());
		PushFrame(high.Generate());
	}

	std::vector<std::pair<uint32_t, uint64_t>> expectedFrames = {
		{1920, 0},
		{1920, TimestampInterval * 1},
		{1920, TimestampInterval * 2},
		{1920, TimestampInterval * 3},
		{1920, TimestampInterval * 4},
	};

	ASSERT_EQ(expectedFrames, PopForwardedFrames());
}


TEST_F(TestSimulcastMediaFrameListener, LayerSelectionOffset) 
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1003, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1006, 30000);
	
	PushFrame(low.Generate(true));
	PushFrame(mid.Generate(true));
	PushFrame(high.Generate(true));

	for (size_t i = 0; i < 14; i++)
	{
		PushFrame(low.Generate());
		PushFrame(mid.Generate());
		PushFrame(high.Generate());
	}

	std::vector<std::pair<uint32_t, uint64_t>> expectedFrames = {
		{480, 0},
		{960, 3},			// Offset caused 
		{1920, 6},
		{1920, 2976},
		{1920, 5946},
		{1920, 8916},
		{1920, 11886},
	};

	ASSERT_EQ(expectedFrames, PopForwardedFrames());
}

TEST_F(TestSimulcastMediaFrameListener, LayerSelectionMissing) 
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1003, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1006, 30000);
	
	PushFrame(low.Generate(true));
	PushFrame(mid.Generate(true));
	PushFrame(high.Generate(true));

	for (size_t i = 0; i < 2; i++)
	{
		PushFrame(low.Generate());
		PushFrame(mid.Generate());
		PushFrame(high.Generate());
	}

	for (size_t i = 0; i < 1; i++)
	{
		PushFrame(low.Generate());
		PushFrame(mid.Generate());
		(void)high.Generate();			// Missing one frame
	}

	for (size_t i = 0; i < 12; i++)
	{
		PushFrame(low.Generate());
		PushFrame(mid.Generate());
		PushFrame(high.Generate());
	}

	std::vector<std::pair<uint32_t, uint64_t>> expectedFrames = {
		{480, 0},
		{960, 3},
		{1920, 6},
		{1920, 2976},
		{1920, 5946},
		{1920, 11886},	// Missing one frame, no switching layer
		{1920, 14856},
	};

	ASSERT_EQ(expectedFrames, PopForwardedFrames());
}


TEST_F(TestSimulcastMediaFrameListener, LayerSelectionOrder) 
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1000, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1000, 30000);

	auto repeatNonIntraFrames = [this, &low, &mid, &high](size_t count){
		for (size_t i = 0; i < count; i++)
		{
			PushFrame(low.Generate());
			PushFrame(mid.Generate());
			PushFrame(high.Generate());
		}
	};
	
	PushFrame(low.Generate(true));
	PushFrame(mid.Generate(true));
	PushFrame(high.Generate(true));

	repeatNonIntraFrames(4);
	
	{
		PushFrame(low.Generate(true));  // Intra frame
		PushFrame(mid.Generate());
		PushFrame(high.Generate());
	}

	repeatNonIntraFrames(9);

	std::vector<std::pair<uint32_t, uint64_t>> expectedFrames = {
		{1920, 0},
		{1920, 2970},
		{1920, 5940},
		{1920, 8910},
		{1920, 11880},
	};

	ASSERT_EQ(expectedFrames, PopForwardedFrames());

	for (size_t i = 0; i < 5; i++)
	{
		PushFrame(low.Generate());
		PushFrame(mid.Generate());
		(void)high.Generate();
	}

	{
		PushFrame(low.Generate(true));	// This intra frame will be discarded as time is not 
		                                // out for high layer despite it is missing frames
		PushFrame(mid.Generate());
		(void)high.Generate();
	}

	for (size_t i = 0; i < 4; i++)
	{
		PushFrame(low.Generate());
		PushFrame(mid.Generate());
		(void)high.Generate();
	}

	{
		PushFrame(low.Generate(true));  // Intra frame, will switch to low layer
		PushFrame(mid.Generate());
		PushFrame(high.Generate());
	}

	repeatNonIntraFrames(1);

	{
		PushFrame(low.Generate());  // Intra frame, will swith to high layer
		PushFrame(mid.Generate());
		PushFrame(high.Generate(true));
	}

	repeatNonIntraFrames(11);

	expectedFrames = {
		{1920, 14850}, 
		{1920, 17820}, 
		{1920, 20790}, 
		{1920, 23760}, 
		{1920, 26730}, 
		{1920, 29700}, 
		{1920, 32670}, 
		{1920, 35640}, 
		{1920, 38610}, 
		{1920, 41580},  // Start missing high layer frames
		{480, 74250},	// Switch to low layer after timeout
		{480, 77220},	// Keep low layer despite of recieving high layer non-intra frames
		{1920, 80190},	// Switch to high layer once get high layer intra frame
		{1920, 83160},
		};

	ASSERT_EQ(expectedFrames, PopForwardedFrames());
}