#include "vp8/vp8depacketizer.h"
#include "VP8TestBase.h"

#include <limits>
#include <memory>

class TestVP8Depacketizer : public VP8TestBase
{
public:

	MediaFrame* Add(std::shared_ptr<RTPPacket> packet)
	{
		return depacketizer.AddPacket(packet);
	}

protected:

	VP8Depacketizer depacketizer;
};

TEST_F(TestVP8Depacketizer, NormalPackets)
{
	// Single partition
	ASSERT_EQ(nullptr, Add(StartPacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));
	ASSERT_NE(nullptr, Add(MarkerPacket(1000, 0)));

	// Multiple partitions
	ASSERT_EQ(nullptr, Add(StartPacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(StartPacket(2000, 1)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 1)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 1)));
	ASSERT_NE(nullptr, Add(MarkerPacket(2000, 1)));
}

TEST_F(TestVP8Depacketizer, NormalPacketsAllZeroPids)
{
	// Multiple partitions with same pid. This is not suggested
	// by RFC7741, but it is allowed.
	ASSERT_EQ(nullptr, Add(StartPacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(StartPacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_NE(nullptr, Add(MarkerPacket(2000, 0)));
}

TEST_F(TestVP8Depacketizer, MissingMiddlePackets)
{
	// First frame has missing packets, will be dropped
	ASSERT_EQ(nullptr, Add(StartPacket(1000, 0)));
	(void)MiddlePacket(1000, 0); // Increase the sequence number. Simulating missing packets
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MarkerPacket(1000, 0)));

	// The following normal packets can be aggregated to a frame
	ASSERT_EQ(nullptr, Add(StartPacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_NE(nullptr, Add(MarkerPacket(2000, 0)));
}

TEST_F(TestVP8Depacketizer, NoStartPacket)
{
	// First frame has no marker packet, will be dropped
	ASSERT_EQ(nullptr, Add(StartPacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));

	// The following normal packets can be aggregated to a frame
	ASSERT_EQ(nullptr, Add(StartPacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_NE(nullptr, Add(MarkerPacket(2000, 0)));
}

TEST_F(TestVP8Depacketizer, NoMarkerPacket)
{
	// First frame has no start packet, will be dropped
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MarkerPacket(1000, 0)));

	// The following normal packets can be aggregated to a frame
	ASSERT_EQ(nullptr, Add(StartPacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(2000, 0)));
	ASSERT_NE(nullptr, Add(MarkerPacket(2000, 0)));
}

TEST_F(TestVP8Depacketizer, NormalPacketsMaxSeqnum)
{
	// Set sequence number to close to max
	currentSeqNum = std::numeric_limits<uint32_t>::max();

	// There is no error reported
	ASSERT_EQ(nullptr, Add(StartPacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));
	ASSERT_EQ(nullptr, Add(MiddlePacket(1000, 0)));
	ASSERT_NE(nullptr, Add(MarkerPacket(1000, 0)));

	ASSERT_EQ(3, currentSeqNum);
}