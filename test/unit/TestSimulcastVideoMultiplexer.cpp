#include "SimulcastVideoMultiplexer.h"

#include <include/gtest/gtest.h>
#include <memory>
#include <optional>
#include <unordered_map>
 

class TestFrameGenerator
{
public:
	static constexpr uint32_t ClockRate = 90 * 1000;
	static constexpr uint64_t TimestampInterval = 2970;
	static constexpr uint64_t FrameTimeIntervalMs = TimestampInterval * 1000 / ClockRate;

	void SetLayerDimension(int32_t ssrc, uint32_t width, uint32_t height)
	{
		layerDimensions[ssrc] = {width, height};
	}

	std::unique_ptr<VideoFrame> GenerateNextFrame(uint32_t ssrc, bool isIntra, uint64_t timestamp, uint64_t frameTime)
	{
		if (layerDimensions.find(ssrc) == layerDimensions.end())
		{
			return nullptr;
		}

		auto [w, h] = layerDimensions[ssrc];
		
		auto frame = std::make_unique<VideoFrame>(VideoCodec::Type::VP8);
		frame->SetSSRC(ssrc);
		frame->SetWidth(w);
		frame->SetHeight(h);
		frame->SetTimestamp(timestamp);
		frame->SetTime(frameTime);

		return std::move(frame);
	}

private:
	
	std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> layerDimensions;
};



TEST(TestSimulcastVideoMultiplexer, LayerSelection) 
{
    SimulcastVideoMultiplexer multiplexer;

	TestFrameGenerator generator;
	generator.SetLayerDimension(1, 480, 270);
	generator.SetLayerDimension(2, 960, 540);
	generator.SetLayerDimension(3, 1920, 1080);

	std::unordered_map<uint32_t, uint64_t> missingFrameCounts;
	std::unordered_map<uint32_t, bool> nextFrameIntra;
	uint64_t currentTime = 0;

	auto tick = [&](){
		currentTime += TestFrameGenerator::FrameTimeIntervalMs;

		for (uint32_t ssrc : {1, 2, 3})
		{
			if (missingFrameCounts[ssrc] > 0)
			{
				missingFrameCounts[ssrc] -= 1;
				continue;
			}

			auto frame = generator.GenerateNextFrame(ssrc, nextFrameIntra[ssrc], currentTime * TestFrameGenerator::ClockRate / 1000, currentTime);

			if (nextFrameIntra[ssrc])
			{
				nextFrameIntra[ssrc] = false;
			}
		}
	};

}
