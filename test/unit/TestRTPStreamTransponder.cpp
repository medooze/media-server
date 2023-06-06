#include "TestCommon.h"
#include "VP8TestBase.h"
#include "rtp/RTPStreamTransponder.h"
#include <limits>

class MockRTPSender : public RTPSender
{
public:
	virtual int Enqueue(const RTPPacket::shared& packet)
	{
		lastPacket = packet;
		return 0;
	};


	inline const RTPPacket::shared& GetLastPacket()
	{
		return lastPacket;
	}

private:
	RTPPacket::shared lastPacket;
};

class MockRTPIncomingMediaStream : public RTPIncomingMediaStream
{
public:
	MockRTPIncomingMediaStream(TimeService& timeService) : timeService(timeService) {};
	virtual void AddListener(Listener* listener) {};
	virtual void RemoveListener(Listener* listener) {};
	virtual DWORD GetMediaSSRC() const { return 0; };
	virtual TimeService& GetTimeService() { return timeService; };
	virtual void Mute(bool muting) {};
private:
	TimeService& timeService;
};

class TestRTPStreamTransponder : public VP8TestBase, public testing::WithParamInterface<std::pair<uint16_t, uint8_t>>
{
public:
	TestRTPStreamTransponder() :
		VP8TestBase(GetParam().first, GetParam().second),
		sender(new MockRTPSender()),
		transponder(std::make_shared<RTPOutgoingSourceGroup>(MediaFrame::Type::Video, timerService), sender),
		stream(new MockRTPIncomingMediaStream(timerService)),
		startPicId(GetParam().first),
		startTl0PicId(GetParam().second)
	{
		//Empty receiver
		RTPReceiver::shared receiver;
		transponder.SetIncoming(stream, receiver);
		transponder.SelectLayer(0, 1);
	};

	void Add(std::shared_ptr<RTPPacket> packet, uint16_t expectedPicId, uint16_t expectedTl0PicIdx)
	{
		transponder.onRTP(stream.get(), packet);
		ASSERT_EQ(expectedPicId, sender->GetLastPacket()->vp8PayloadDescriptor->pictureId);
		ASSERT_EQ(expectedTl0PicIdx, sender->GetLastPacket()->vp8PayloadDescriptor->temporalLevelZeroIndex);
	}

	constexpr uint16_t GetExpectedPicId(int offset)
	{
		return startPicId + offset;
	}

	constexpr uint8_t GetExpectedTl0PicId(int offset)
	{
		return startTl0PicId + offset;
	}


protected:

	TestTimeService timerService;
	std::shared_ptr<MockRTPSender> sender;
	RTPStreamTransponder transponder;

	std::shared_ptr<MockRTPIncomingMediaStream> stream;

	uint16_t startPicId = 0;
	uint8_t startTl0PicId = 0;
};

TEST_P(TestRTPStreamTransponder, NoDropping)
{
	// Single partition
	ASSERT_NO_FATAL_FAILURE(Add(StartPacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));
	ASSERT_NO_FATAL_FAILURE(Add(MarkerPacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));

	// Multiple partitions
	ASSERT_NO_FATAL_FAILURE(Add(StartPacket(2000, 0), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(2000, 0), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(2000, 0), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(2000, 0), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(StartPacket(2000, 1), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(2000, 1), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(2000, 1), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MarkerPacket(2000, 1), GetExpectedPicId(2), GetExpectedTl0PicId(2)));

	// Single partition, not base layer
	ASSERT_NO_FATAL_FAILURE(Add(StartPacket(3000, 0, 1), GetExpectedPicId(3), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(3000, 0, 1), GetExpectedPicId(3), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(3000, 0, 1), GetExpectedPicId(3), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MarkerPacket(3000, 0, 1), GetExpectedPicId(3), GetExpectedTl0PicId(2)));
}


TEST_P(TestRTPStreamTransponder, Dropping)
{
	// Single partition
	ASSERT_NO_FATAL_FAILURE(Add(StartPacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));
	ASSERT_NO_FATAL_FAILURE(Add(MarkerPacket(1000, 0), GetExpectedPicId(1), GetExpectedTl0PicId(1)));

	// Generate packets and not add it to simulate dropping
	(void)StartPacket(2000, 0);
	(void)MiddlePacket(2000, 0);
	(void)MarkerPacket(2000, 0);

	ASSERT_EQ(GetExpectedPicId(2), currentPicId);
	ASSERT_EQ(GetExpectedTl0PicId(2), currentTl0PicId);

	// Frame without marker packet still increases pic ID
	ASSERT_NO_FATAL_FAILURE(Add(StartPacket(2050, 0), GetExpectedPicId(2), GetExpectedTl0PicId(2)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(2050, 0), GetExpectedPicId(2), GetExpectedTl0PicId(2)));

	ASSERT_EQ(GetExpectedPicId(3), currentPicId);
	ASSERT_EQ(GetExpectedTl0PicId(3), currentTl0PicId);

	// The pic ID and layer 0 pic index is still continous
	ASSERT_NO_FATAL_FAILURE(Add(StartPacket(3000, 0), GetExpectedPicId(3), GetExpectedTl0PicId(3)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(3000, 0), GetExpectedPicId(3), GetExpectedTl0PicId(3)));
	ASSERT_NO_FATAL_FAILURE(Add(MiddlePacket(3000, 0), GetExpectedPicId(3), GetExpectedTl0PicId(3)));
	ASSERT_NO_FATAL_FAILURE(Add(MarkerPacket(3000, 0), GetExpectedPicId(3), GetExpectedTl0PicId(3)));
}

INSTANTIATE_TEST_SUITE_P(TestVP8LayerSelectorCases,
	TestRTPStreamTransponder,
	testing::Values(std::pair(0, 0),
		std::pair(100, 50),
		std::pair(std::numeric_limits<uint16_t>::max(), std::numeric_limits<uint8_t>::max())));