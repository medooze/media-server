#include "vp8/vp8depacketizer.h"
#include "TestCommon.h"

#include <limits>
#include <memory>

class TestVP8Depacketizer : public ::testing::Test
{
public:

    static constexpr uint32_t TEST_SSRC = 0x1234;

    std::shared_ptr<RTPPacket> StartPacket(uint64_t tm, uint8_t pid)
    {
        return CreatePacket(tm, currentSeqNum++, true, pid, false);
    }

    std::shared_ptr<RTPPacket> MiddlePacket(uint64_t tm,  uint8_t pid)
    {
        return CreatePacket(tm, currentSeqNum++, false, pid, false);
    }

    std::shared_ptr<RTPPacket> MarkerPacket(uint64_t tm, uint8_t pid)
    {
        return CreatePacket(tm, currentSeqNum++, false, pid, true);
    }

    MediaFrame* Add(std::shared_ptr<RTPPacket> packet)
    {
        return depacketizer.AddPacket(packet);
    }

protected:

    std::shared_ptr<RTPPacket> CreatePacket(uint64_t tm, uint32_t seqnum, bool startOfPartition, uint8_t pid, bool mark)
    {
        std::shared_ptr<RTPPacket> packet = std::make_shared<RTPPacket>(MediaFrame::Type::Video, VideoCodec::VP8);

        packet->SetExtTimestamp(tm);
        packet->SetSSRC(TEST_SSRC);
        packet->SetExtSeqNum(seqnum);
        packet->SetMark(mark);

        constexpr size_t BufferSize = 1024;
        unsigned char buffer[BufferSize];
        memset(buffer, 0, BufferSize);

        VP8PayloadDescriptor desc(startOfPartition, pid);
        auto len = desc.Serialize(buffer, BufferSize);
        packet->SetPayload(buffer, BufferSize);
        packet->AdquireMediaData();

        return packet;
    }

    VP8Depacketizer depacketizer;

    uint32_t currentSeqNum = 0;
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