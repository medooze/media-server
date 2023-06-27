#include <gtest/gtest.h>

#include "data/FrameInfoData.h"
#include "MediaFrameListenerBridge.h"

class TestDefaultPacketDispatchCoordinator : public testing::Test
{
public:
	TestDefaultPacketDispatchCoordinator() : 
		timeGetter(),
		dispatch(timeGetter)
	{
	}

	void TestDispatch(const std::vector<std::tuple<MediaFrame::Type, uint64_t, uint64_t, uint32_t>>& framesInfo,
			  MediaFrame::Type testingMediaType);

protected:
	
	class MockTimeGetter : public TimeGetterInterface
	{
	public:
		virtual QWORD GetTimeUs() override
		{
			return now;
		}

		virtual QWORD GetTimeDiffUs(QWORD time) override
		{
			assert(now >= time);
			return now - time;
		}
		
		void SetTimeUs(QWORD time)
		{
			now = time;
		}
		
		QWORD now = 0;
	};
	
	void Dispatch();
	
	MockTimeGetter timeGetter;
	MediaFrameListenerBridge::DefaultPacketDispatchTimeCoordinator dispatch;
	
	std::queue<std::pair<RTPPacket::shared, std::chrono::milliseconds>> packets;	
	uint64_t lastSentTimeMs = 0;
	
	std::vector<std::pair<uint64_t, uint64_t>> dispatchedPackets;
};

void TestDefaultPacketDispatchCoordinator::TestDispatch(const std::vector<std::tuple<MediaFrame::Type, uint64_t, uint64_t, uint32_t>>& framesInfo, 
							MediaFrame::Type testingMediaType)
{
	// Loop through all frames
	for (auto& f : framesInfo)
	{	
		auto mediaType = std::get<0>(f);
		if (mediaType != testingMediaType) continue;
		
		// Simulcating dispatching between frame arriving
		auto nextArrivalTimsMs = std::get<1>(f);		
		while (!packets.empty() && (timeGetter.GetTimeUs()/1000) < nextArrivalTimsMs)
		{
			Dispatch();
			
			// Advance 1ms
			timeGetter.SetTimeUs(timeGetter.GetTimeUs() + 3000);
		}
		
		timeGetter.SetTimeUs(nextArrivalTimsMs * 1000);
				
		dispatch.OnFrameArrival(mediaType, 
					std::chrono::milliseconds(nextArrivalTimsMs),
					std::get<2>(f),
					std::get<3>(f));
					
		auto packetCount = mediaType == MediaFrame::Audio ? 1 : 10;
		// Test smooth dispatching for video
		auto smooth = mediaType == MediaFrame::Video;
		for (size_t i = 0; i < packetCount; i++)
		{
			// Just add one packet for one frame.
			RTPPacket::shared packet = std::make_shared<RTPPacket>(mediaType, 
									       mediaType == MediaFrame::Audio ? uint8_t(AudioCodec::AAC) : uint8_t(VideoCodec::H264), 
									       nextArrivalTimsMs);
			packet->SetExtTimestamp(std::get<2>(f));
			packet->SetClockRate(std::get<3>(f));
					
			dispatch.OnPacket(mediaType, timeGetter.GetTimeUs(), std::get<2>(f), std::get<3>(f));
			
			auto element = std::make_pair<>(packet,  std::chrono::milliseconds(smooth ? 3 : 0));		
			packets.emplace(element);
		}
	}
	
	while (!packets.empty())
	{
		Dispatch();
		
		// Advance 3ms for testing
		timeGetter.SetTimeUs(timeGetter.GetTimeUs() + 3000);
	}
}


void TestDefaultPacketDispatchCoordinator::Dispatch()
{
	auto now = timeGetter.GetTimeUs() / 1000;
	
	ASSERT_GT(now, lastSentTimeMs);
	auto period = lastSentTimeMs != 0 ? now - lastSentTimeMs : 1;
	
	auto [sending, unused] = dispatch.GetPacketsToDispatch(packets, std::chrono::milliseconds(period), std::chrono::milliseconds(now));
	
	for (auto &s : sending)
	{
		auto ts = PacketDispatchCoordinator::convertTimestampClockRate(s->GetExtTimestamp(), s->GetClockRate(), 90000);		
		lastSentTimeMs = now;
		
		dispatchedPackets.emplace_back(now, ts);
	}
}


TEST_F(TestDefaultPacketDispatchCoordinator, testDispatchAudio)
{
	std::vector<std::pair<uint64_t, uint64_t>> references;
	ASSERT_NO_FATAL_FAILURE(TestDispatch(TestData::FramesInfoData, MediaFrame::Audio));

	ASSERT_EQ(1687762804437, dispatchedPackets.back().first);
	ASSERT_EQ(896640, dispatchedPackets.back().second);
	
	std::vector<std::pair<uint64_t, uint64_t>> expectedFirstTenDispached =
	{
		{1687762794623, 0}, 
		{1687762794623, 1920}, 
		{1687762794623, 3840}, 
		{1687762794623, 5760}, 
		{1687762794623, 7680}, 
		{1687762794633, 9600}, 
		{1687762794633, 11520}, 
		{1687762794633, 13440}, 
		{1687762794786, 15360}, 
		{1687762794786, 17280}
	};
	
	std::vector<std::pair<uint64_t, uint64_t>> actualFirstTenDispatched(dispatchedPackets.begin(), dispatchedPackets.begin() + 10);
	ASSERT_EQ(expectedFirstTenDispached, actualFirstTenDispatched);
	
	dispatch.Reset();
	
	auto offset = std::get<1>(TestData::FramesInfoData.back()) - std::get<1>(TestData::FramesInfoData.front()) + 1000;
	auto testFramesInfo2 = TestData::FramesInfoData;
	for (auto& f : testFramesInfo2)
	{
		// Simulcating frames arrived later
		std::get<1>(f) += offset;
	}
	
	ASSERT_NO_FATAL_FAILURE(TestDispatch(testFramesInfo2, MediaFrame::Audio));
	
	ASSERT_EQ(1687762815353, dispatchedPackets.back().first);
	ASSERT_EQ(1892460, dispatchedPackets.back().second);
}


TEST_F(TestDefaultPacketDispatchCoordinator, testDispatchVideo)
{
	std::vector<std::pair<uint64_t, uint64_t>> references;
	ASSERT_NO_FATAL_FAILURE(TestDispatch(TestData::FramesInfoData, MediaFrame::Video));

	ASSERT_EQ(1687762804539, dispatchedPackets.back().first);
	ASSERT_EQ(867869, dispatchedPackets.back().second);
	
	std::vector<std::pair<uint64_t, uint64_t>> expectedFirstTenDispached =
	{
		{1687762794880, 0}, 
		{1687762794883, 0}, 
		{1687762794883, 0}, 
		{1687762794886, 0}, 
		{1687762794886, 0}, 
		{1687762794889, 0}, 
		{1687762794889, 0}, 
		{1687762794892, 0}, 
		{1687762794892, 0}, 
		{1687762794895, 0}
	};
	
	std::vector<std::pair<uint64_t, uint64_t>> actualFirstTenDispatched(dispatchedPackets.begin(), dispatchedPackets.begin() + 10);
	ASSERT_EQ(expectedFirstTenDispached, actualFirstTenDispatched);
}
