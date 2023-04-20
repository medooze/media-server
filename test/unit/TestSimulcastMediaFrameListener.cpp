#include "SimulcastMediaFrameListener.h"
#include "video.h"

#include "TestCommon.h"

#include <future>
#include <memory>
#include <sstream>
#include <fstream>
#include <regex>
#include <unordered_map>

namespace {
static constexpr uint32_t ClockRate = 90 * 1000;
static constexpr uint64_t TimestampInterval = 2970;
static constexpr uint64_t FrameTimeIntervalMs = TimestampInterval * 1000 / ClockRate;
}

struct FrameInput
{
	uint32_t width;
	uint32_t intra;
	uint64_t timestamp;
	uint64_t time;
};

class TestFrameGenerator
{
public:
	TestFrameGenerator(uint32_t ssrc, uint32_t width, uint32_t height, uint64_t initialTime, uint64_t initialTimestamp) :
		ssrc(ssrc),
		width(width),
		height(height),
		initialFrameTime(initialTime),
		initialTimestamp(initialTimestamp)
	{
	}

	std::unique_ptr<VideoFrame> Generate(bool isIntra = false)
	{
		auto frame = std::make_unique<VideoFrame>(VideoCodec::Type::VP8);
		frame->SetSSRC(ssrc);
		frame->SetWidth(width);
		frame->SetHeight(height);
		frame->SetTimestamp(initialTimestamp + TimestampInterval * ticks);
		frame->SetTime(initialFrameTime + FrameTimeIntervalMs * ticks);
		frame->SetIntra(isIntra);
		frame->SetLength(width * height);
		frame->SetClockRate(ClockRate);

		ticks++;

		return std::move(frame);
	}

	void AlignTo(const TestFrameGenerator& other)
	{
		ticks = other.ticks;
	}

	void Reset()
	{
		ticks = 0;
	}

private:
	uint32_t ssrc;
	uint32_t width;
	uint32_t height;

	uint64_t initialFrameTime;
	uint64_t initialTimestamp;

	uint32_t ticks = 0;
};

class FramePushHelper
{
public:
	static constexpr uint8_t	LOW = 0x1;
	static constexpr uint8_t	MID = 0x2;
	static constexpr uint8_t	HIGH = 0x4;
	static constexpr uint8_t	INTRA = 0x8;
	static constexpr uint8_t	ALL_NON_INTRA = LOW | MID | HIGH;
	static constexpr uint8_t	ALL_INTRA = LOW | MID | HIGH | INTRA;

	FramePushHelper(SimulcastMediaFrameListener& listener, TestFrameGenerator &low, TestFrameGenerator &mid, TestFrameGenerator &high) :
		listener(listener),
		low(low),
		mid(mid),
		high(high)
	{
	}

	void PushAdvance(uint8_t type, size_t repeats = 1)
	{
		bool intra = type & INTRA;

		for (size_t i = 0; i < repeats; i++)
		{
			if (type & LOW)
			{
				auto frame = low.Generate(intra);
				listener.onMediaFrame(frame->GetSSRC(), *frame);
			}

			if (type & MID)
			{
				auto frame = mid.Generate(intra);
				listener.onMediaFrame(frame->GetSSRC(), *frame);
			}

			if (type & HIGH)
			{
				auto frame = high.Generate(intra);
				listener.onMediaFrame(frame->GetSSRC(), *frame);
			}
		}
	}

private:

	TestFrameGenerator &low;
	TestFrameGenerator &mid;
	TestFrameGenerator &high;
	SimulcastMediaFrameListener &listener;
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
		forwardedFrames.emplace_back(dynamic_cast<const VideoFrame&>(frame).GetWidth(),
									frame.GetTimeStamp(),
									frame.GetTime());
	}

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> forwardedFrames;
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

	void PushFrame(const FrameInput& input)
	{
		auto frame = std::make_unique<VideoFrame>(VideoCodec::Type::VP8);
		frame->SetSSRC(input.width);
		frame->SetWidth(input.width);
		frame->SetHeight(input.width);
		frame->SetTimestamp(input.timestamp);
		frame->SetTime(input.time);
		frame->SetIntra(input.intra);
		frame->SetLength(input.width*input.width);
		frame->SetClockRate(ClockRate);

		listener.onMediaFrame(frame->GetSSRC(), *frame);
	}

	void CheckResetForwardedFrames(const std::vector<std::tuple<uint32_t, uint64_t, uint64_t>>& expected)
	{
		auto forwarded = std::move(mediaFrameListener->forwardedFrames);
#ifdef CHECK_WIDTH_ONLY
		ASSERT_GE(forwarded.size(), expected.size());
		size_t count = expected.size();
		for (size_t i = 0; i < count; i++)
		{
			ASSERT_EQ(std::get<0>(expected[i]), std::get<0>(forwarded[i])) << "index:" << i << "\n"
					<< "expected: width:" << std::get<0>(expected[i]) << ",\ttimestamp:" << std::get<1>(expected[i]) << ",\ttime:" << std::get<2>(expected[i]) << "\n"
					<< "  actual: width:" << std::get<0>(forwarded[i]) << ",\ttimestamp:" << std::get<1>(forwarded[i]) << ",\ttime:" << std::get<2>(forwarded[i]);
		}
#else
		ASSERT_EQ(expected, forwarded);
#endif
	}

	void CheckResetForwardedFrames(uint32_t expectedLayerWidth)
	{
		auto forwarded = std::move(mediaFrameListener->forwardedFrames);
		for (auto &frame : forwarded)
		{
			ASSERT_EQ(expectedLayerWidth, std::get<0>(frame)) << "timestamp:" << std::get<1>(frame) << ",time:" << std::get<2>(frame);
		}
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

	FramePushHelper fp(listener, low, mid, high);
	fp.PushAdvance(FramePushHelper::ALL_INTRA);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
		{1920, 0, 1000},
		{1920, 2970, 1033}
	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));

	// Change to just one layer
	listener.SetNumLayers(1);
	low.Reset();

	// Test the frame will be forwarded immediately
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA);
	expectedFrames = {{{480, 0, 1000}}};
	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));

	// Change to two layers
	listener.SetNumLayers(2);
	low.Reset();
	high.Reset();

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA);
	expectedFrames.clear();
	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));

	fp.PushAdvance(FramePushHelper::HIGH | FramePushHelper::INTRA);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::HIGH, 5);
	expectedFrames = {
		{1920, 0, 1000},
		{1920, 2970, 1033},
		{1920, 5940, 1066},
		{1920, 8910, 1099},
		{1920, 11880, 1132},
		{1920, 14850, 1165}
	};
	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}


TEST_F(TestSimulcastMediaFrameListener, LayerSelectionOffset)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1003, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1006, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::ALL_INTRA);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 2);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
	 	{1920, 540, 1006},
		{1920, 3510, 1039}
	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}

TEST_F(TestSimulcastMediaFrameListener, LayerSelectionNegativeOffset)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   997, 20000);
	TestFrameGenerator high(3, 1920, 1080, 990, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::ALL_INTRA);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
	 	{1920, 0, 990},
		{1920, 2070, 1023}
	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}

TEST_F(TestSimulcastMediaFrameListener, LayerSelectionMissing)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1003, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1006, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::ALL_INTRA);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 2);

	fp.PushAdvance(FramePushHelper::LOW);
	fp.PushAdvance(FramePushHelper::MID);

	high.AlignTo(low);

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 6);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
		{1920, 540, 1006},
		{1920, 3510, 1039},
		{1920, 6480, 1072},
		{1920, 12420, 1138},	// Missing one frame, no switching layer
		{1920, 15390, 1171},
		{1920, 18360, 1204},
		{1920, 21330, 1237},
		{1920, 24300, 1270}
	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}


TEST_F(TestSimulcastMediaFrameListener, LayerSelectionOrder)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1000, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1000, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::ALL_INTRA);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 4);
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA);  // Intra frame
	fp.PushAdvance(FramePushHelper::MID | FramePushHelper::HIGH);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 9);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
		{1920, 0, 1000},
		{1920, 2970, 1033},
		{1920, 5940, 1066},
		{1920, 8910, 1099},
		{1920, 11880, 1132},
		{1920, 14850, 1165},
		{1920, 17820, 1198},
		{1920, 20790, 1231},
		{1920, 23760, 1264},
		{1920, 26730, 1297},
		{1920, 29700, 1330},
		{1920, 32670, 1363},
		{1920, 35640, 1396},
		{1920, 38610, 1429},
		{1920, 41580, 1462}
	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID, 5);

	// This intra frame will be discarded as time is not
	// out for high layer despite it is missing frames
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA);
	fp.PushAdvance(FramePushHelper::MID);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID, 4);

	// Intra frame, will switch to low layer
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA);
	fp.PushAdvance(FramePushHelper::MID);

	high.AlignTo(low);

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID);
	fp.PushAdvance(FramePushHelper::HIGH | FramePushHelper::INTRA);

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);

	expectedFrames = {
		{480, 74250, 1825},	  // Switch to low layer after timeout
		{480, 77220, 1858},   // Keep low layer despite of recieving high layer non-intra frames
		{1920, 80190, 1891},  // Switch to high layer once get high layer intra frame
		{1920, 83160, 1924}
		};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}

TEST_F(TestSimulcastMediaFrameListener, NoHighLayerAtBeginning)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1000, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1000, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA);  // Intra frame
	fp.PushAdvance(FramePushHelper::MID | FramePushHelper::INTRA);  // Intra frame

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID, 11);

	high.AlignTo(low);

	fp.PushAdvance(FramePushHelper::HIGH | FramePushHelper::INTRA);  // Intra frame
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID);

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
		{960, 0, 1000},
		{960, 2970, 1033},
		{960, 5940, 1066},
		{960, 8910, 1099},
		{960, 11880, 1132},
		{960, 14850, 1165},
		{960, 17820, 1198},
		{960, 20790, 1231},
		{960, 23760, 1264},
		{960, 26730, 1297},
		{960, 29700, 1330},
		{960, 32670, 1363},
		{1920, 35640, 1396},
		{1920, 38610, 1429}
 	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}

TEST_F(TestSimulcastMediaFrameListener, NoHighLayerIntraAtBeginning)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1000, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1000, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA);  // Intra frame
	fp.PushAdvance(FramePushHelper::MID | FramePushHelper::INTRA);  // Intra frame

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 11);

	high.AlignTo(low);

	fp.PushAdvance(FramePushHelper::HIGH | FramePushHelper::INTRA);  // Intra frame
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID);

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
		{960, 0, 1000},
		{960, 2970, 1033},
		{960, 5940, 1066},
		{960, 8910, 1099},
		{960, 11880, 1132},
		{960, 14850, 1165},
		{960, 17820, 1198},
		{960, 20790, 1231},
		{960, 23760, 1264},
		{960, 26730, 1297},
		{960, 29700, 1330},
		{960, 32670, 1363},
		{1920, 35640, 1396},
		{1920, 38610, 1429}
 	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}

TEST_F(TestSimulcastMediaFrameListener, HighLayerRemovedInMiddle)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1000, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1000, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::ALL_INTRA, 1);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 20);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID, 10);
	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(1920));

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID | FramePushHelper::INTRA);
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID, 10);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
		{960, 92070, 2023}
 	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));

	high.AlignTo(low);

	fp.PushAdvance(FramePushHelper::HIGH | FramePushHelper::INTRA);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);

	expectedFrames = {
		{960, 95040, 2056},
		{960, 98010, 2089},
		{960, 100980, 2122},
		{960, 103950, 2155},
		{960, 106920, 2188},
		{960, 109890, 2221},
		{960, 112860, 2254},
		{960, 115830, 2287},
		{960, 118800, 2320},
		{960, 121770, 2353},
		{1920, 124740, 2386},
		{1920, 127710, 2419}
 	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}

TEST_F(TestSimulcastMediaFrameListener, MidLayerIntraframe)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1000, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1000, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::ALL_INTRA, 1);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 20);

	fp.PushAdvance(FramePushHelper::MID | FramePushHelper::INTRA); // An mid layer iframe
	fp.PushAdvance(FramePushHelper::HIGH | FramePushHelper::LOW);

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(1920));
	fp.PushAdvance(FramePushHelper::LOW, 10);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));

	mid.AlignTo(low);
	fp.PushAdvance(FramePushHelper::MID | FramePushHelper::INTRA); // An mid layer iframe

	high.AlignTo(mid);
	low.AlignTo(mid);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);
	// It wouldn't forward mid layer immediately
	expectedFrames = {};
	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 9);

	expectedFrames = {
		{960, 95040, 2056},
		{960, 98010, 2089},
		{960, 100980, 2122},
		{960, 103950, 2155},
		{960, 106920, 2188},
		{960, 109890, 2221},
		{960, 112860, 2254},
		{960, 115830, 2287},
		{960, 118800, 2320},
		{960, 121770, 2353}
 	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}


TEST_F(TestSimulcastMediaFrameListener, HighLayerIFrames)
{
	TestFrameGenerator low(1, 480, 270,   1000, 10000);
	TestFrameGenerator mid(2, 960, 540,   1003, 20000);
	TestFrameGenerator high(3, 1920, 1080, 1006, 30000);

	FramePushHelper fp(listener, low, mid, high);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::INTRA); // An lower layer iframe
	fp.PushAdvance(FramePushHelper::MID);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID, 10);

	fp.PushAdvance(FramePushHelper::MID | FramePushHelper::INTRA);
	fp.PushAdvance(FramePushHelper::LOW);

	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID | FramePushHelper::MID, 5);
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID | FramePushHelper::INTRA);

	high.AlignTo(low);
	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 10); // An mid layer iframe

	fp.PushAdvance(FramePushHelper::HIGH | FramePushHelper::INTRA);
	fp.PushAdvance(FramePushHelper::LOW | FramePushHelper::MID);

	fp.PushAdvance(FramePushHelper::ALL_NON_INTRA, 1);

	std::vector<std::tuple<uint32_t, uint64_t, uint64_t>> expectedFrames = {
		{480, 0, 1000},
		{480, 2970, 1033},
		{480, 5940, 1066},
		{480, 8910, 1099},
		{480, 11880, 1132},
		{480, 14850, 1165},
		{480, 17820, 1198},
		{480, 20790, 1231},
		{480, 23760, 1264},
		{480, 26730, 1297},
		{480, 29700, 1330},
		{960, 32940, 1366},
		{960, 35910, 1399},
		{960, 38880, 1432},
		{960, 41850, 1465},
		{960, 44820, 1498},
		{960, 47790, 1531},
		{960, 50760, 1564},
		{960, 53730, 1597},
		{960, 56700, 1630},
		{960, 59670, 1663},
		{960, 62640, 1696},
		{960, 65610, 1729},
		{960, 68580, 1762},
		{960, 71550, 1795},
		{960, 74520, 1828},
		{960, 77490, 1861},
		{960, 80460, 1894},
		{1920, 83700, 1930}
 	};

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(expectedFrames));
}


TEST_F(TestSimulcastMediaFrameListener, ActualLogProcess)
{
	// Comment for testing
	GTEST_SKIP();

	std::vector<std::unique_ptr<VideoFrame>> frames;
	std::regex reg(R"(.*SimulcastMediaFrameListener::onMediaFrame.*\[([0-9]+),([0-9]+),([0-9]+),([0-9]+)\])");

	std::unordered_map<uint32_t, uint32_t> dims;
	dims[2783539029] = 1920;   // L
	dims[3354030150] = 960;    // M
	dims[1176922389] = 480;	   // S

	size_t counter = 0;
	std::ifstream infile("old_alg_final_2.log");
	std::string line;
	while (std::getline(infile, line))
	{
		std::smatch match;
		if (std::regex_search(line, match, reg))
		{
			auto frame = std::make_unique<VideoFrame>(VideoCodec::Type::VP8);
			auto ssrc = std::stoull(match[1]);
			frame->SetSSRC(ssrc);
			frame->SetIntra(std::stoull(match[2]));

			frame->SetTimestamp(std::stoull(match[3]));
			frame->SetTime(std::stoull(match[4]));

			ASSERT_TRUE(dims.find(frame->GetSSRC()) != dims.end());
			frame->SetWidth(dims[ssrc]);
			frame->SetHeight(dims[ssrc]);
			frame->SetLength(dims[ssrc] * dims[ssrc]);

			if (frame->GetTime() > 1677727591998)
			{
				//std::cout << "{" << dims[ssrc] << "," << match[2] << "," << match[3] << "," << match[4] << "}," << std::endl;
				PushFrame(std::move(frame));
				if (counter++ == 1500) break;
			}
		}
	}

	std::ofstream outfile("output.csv");
	for (auto [ssrc, timestamp, time] : mediaFrameListener->forwardedFrames)
	{
		outfile << ssrc << "," << timestamp << "," << time << std::endl;
	}
}


TEST_F(TestSimulcastMediaFrameListener, ActualDataInput)
{
	std::vector<FrameInput> inputs = {
		{1920,1,1924004629,1677727601995},
		{480,0,3469462456,1677727602023},
		{960,0,233967998,1677727602024},
		{1920,0,1924007599,1677727602029},
		{480,0,3469465516,1677727602064},
		{480,0,3469468486,1677727602096},
		{960,0,233971058,1677727602065},
		{960,0,233974028,1677727602100},
		{1920,0,1924010659,1677727602059},
		{480,0,3469471546,1677727602146},
		{960,0,233977088,1677727602146},
		{1920,0,1924013629,1677727602124},
		{480,0,3469474516,1677727602152},
		{960,0,233980058,1677727602152},
		{1920,0,1924016689,1677727602151},
		{1920,0,1924019659,1677727602168},
		{480,0,3469477486,1677727602200},
		{960,0,233983028,1677727602200},
		{480,0,3469480456,1677727602220},
		{1920,1,1924022629,1677727602199},
		{480,0,3469483516,1677727602262},
		{480,0,3469486486,1677727602314},
		{960,0,233985998,1677727602229},
		{480,0,3469489456,1677727602328},
		{480,1,3469492516,1677727602396},
		{960,0,233989058,1677727602358},
		{960,0,233992028,1677727602470},
		{1920,0,1924025599,1677727602298},
		{960,0,233994998,1677727602474},
		{480,0,3469522486,1677727602678},
		{1920,0,1924028659,1677727602516},
		{1920,0,1924031629,1677727602698},
		{960,1,233998058,1677727602577},
		{960,0,234016058,1677727602705},
		{480,0,3469528516,1677727602748},
		{960,0,234028028,1677727602748},
		{960,0,234037028,1677727602798},
		{960,0,234043058,1677727602850},
		{1920,0,1924034599,1677727602700},
		{960,0,234046028,1677727602899},
		{1920,1,1924037659,1677727602896},
		{480,0,3469558486,1677727603072},
		{1920,0,1924055659,1677727603067},
		{960,0,234066998,1677727603120},
		{480,0,3469567486,1677727603196},
		{1920,0,1924061599,1677727603120},
		{960,0,234075998,1677727603209},
		{1920,0,1924079599,1677727603201},
		{480,0,3469573516,1677727603261},
		{960,0,234079058,1677727603261},
		{960,0,234082028,1677727603298},
		{1920,0,1924091659,1677727603253},
		{480,0,3469582516,1677727603353},
		{960,0,234091028,1677727603398},
		{1920,0,1924097599,1677727603347},
		{480,0,3469588456,1677727603420},
		{1920,0,1924100659,1677727603408},
		{1920,0,1924118659,1677727603451},
		{960,0,234102998,1677727603511},
		{1920,0,1924121629,1677727603497},
		{960,0,234106058,1677727603550},
		{1920,0,1924124599,1677727603549},
		{960,0,234109028,1677727603597},
		{480,0,3469606456,1677727603615},
		{1920,0,1924130629,1677727603597},
		{960,0,234115058,1677727603666},
		{1920,0,1924142599,1677727603649},
		{1920,0,1924151599,1677727603696},
	};

	for (auto& input : inputs)
	{
		PushFrame(input);
	}

	ASSERT_NO_FATAL_FAILURE(CheckResetForwardedFrames(1920));
}