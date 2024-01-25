#include "FecProbeGenerator.h"
#include "RtpTestCommon.h"
#include <gtest/gtest.h>
#include "tools.h"
#include "codecs.h"

constexpr WORD kMaxFecRate = 255;

class TestFecProbeGenerator : public testing::Test
{
public:
	TestFecProbeGenerator()
	{
		Logger::EnableDebug(true);

		auto codec = VideoCodec::GetCodecForName("H264");
		rtpMap.SetCodecForType(kPayloadType, codec);
	}

	RTPMap rtpMap;
	RTPMap extMap;
	RTPPacketGenerator rtpPacketGenerator{rtpMap, extMap};
	CircularQueue<RTPPacket::shared> history;
	FecProbeGenerator fecGenerator{history, extMap};
};

const BYTE* GetFecHeaderOfNthFecPacket(const std::vector<RTPPacket::shared>& fecPackets, size_t n)
{
	EXPECT_LT(n, fecPackets.size());
	return (*(std::next(fecPackets.begin(), n)))->GetMediaData();
}

TEST_F(TestFecProbeGenerator, ShouldSetLastProtectedSsrcAfterCorrectFecEncoding)
{
	history.push_back(rtpPacketGenerator.Create());

	EXPECT_FALSE(fecGenerator.GetLastProtectedSsrc().has_value());

	auto fecPackets = fecGenerator.PrepareFecProbePackets(0, true);

	EXPECT_TRUE(fecGenerator.GetLastProtectedSsrc().has_value());
	EXPECT_EQ(kSsrc, fecGenerator.GetLastProtectedSsrc().value());
}

TEST_F(TestFecProbeGenerator, ShouldProduceFecPacketsAccordingToFecRate)
{
	history.push_back(rtpPacketGenerator.Create(0));
	history.push_back(rtpPacketGenerator.Create(1));
	history.push_back(rtpPacketGenerator.Create(2));
	auto fecPackets = fecGenerator.PrepareFecProbePackets(kMaxFecRate, 0);

	EXPECT_EQ(3, fecPackets.size());
	EXPECT_EQ(kSeqNum, get2(GetFecHeaderOfNthFecPacket(fecPackets, 0), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 0)[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(kSeqNum + 1, get2(GetFecHeaderOfNthFecPacket(fecPackets, 1), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 1)[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(kSeqNum + 2, get2(GetFecHeaderOfNthFecPacket(fecPackets, 2), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 2)[18] & (0x7f))); // Mask [0-6]


	history.push_back(rtpPacketGenerator.Create(3));
	history.push_back(rtpPacketGenerator.Create(4));
	history.push_back(rtpPacketGenerator.Create(5));
	fecPackets = fecGenerator.PrepareFecProbePackets(2 * kMaxFecRate / 3, 0);

	EXPECT_EQ(2, fecPackets.size());
	EXPECT_EQ(kSeqNum + 3, get2(GetFecHeaderOfNthFecPacket(fecPackets, 0), 16)); // SN base_0
	EXPECT_EQ(0b1010000, (GetFecHeaderOfNthFecPacket(fecPackets, 0)[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(kSeqNum + 4, get2(GetFecHeaderOfNthFecPacket(fecPackets, 1), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 1)[18] & (0x7f))); // Mask [0-6]


	history.push_back(rtpPacketGenerator.Create(6));
	history.push_back(rtpPacketGenerator.Create(7));
	history.push_back(rtpPacketGenerator.Create(8));
	fecPackets = fecGenerator.PrepareFecProbePackets(kMaxFecRate / 3, 0);

	EXPECT_EQ(1, fecPackets.size());
	EXPECT_EQ(kSeqNum + 6, get2(GetFecHeaderOfNthFecPacket(fecPackets, 0), 16)); // SN base_0
	EXPECT_EQ(0b1110000, (GetFecHeaderOfNthFecPacket(fecPackets, 0)[18] & (0x7f))); // Mask [0-6]
}

TEST_F(TestFecProbeGenerator, ShouldReturnDuplicatesWhenFecRateIsHigherThan100Percent)
{
	history.push_back(rtpPacketGenerator.Create(0));
	history.push_back(rtpPacketGenerator.Create(1));
	history.push_back(rtpPacketGenerator.Create(2));

	auto fecPackets = fecGenerator.PrepareFecProbePackets(2 * kMaxFecRate + 80, 0);
	EXPECT_EQ(7, fecPackets.size());
	EXPECT_EQ(kSeqNum + 0, get2(GetFecHeaderOfNthFecPacket(fecPackets, 0), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 0)[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(kSeqNum + 0, get2(GetFecHeaderOfNthFecPacket(fecPackets, 1), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 1)[18] & (0x7f))); // Mask [0-6]

	EXPECT_EQ(kSeqNum + 1, get2(GetFecHeaderOfNthFecPacket(fecPackets, 2), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 2)[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(kSeqNum + 1, get2(GetFecHeaderOfNthFecPacket(fecPackets, 3), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 3)[18] & (0x7f))); // Mask [0-6]

	EXPECT_EQ(kSeqNum + 2, get2(GetFecHeaderOfNthFecPacket(fecPackets, 4), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 4)[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(kSeqNum + 2, get2(GetFecHeaderOfNthFecPacket(fecPackets, 5), 16)); // SN base_0
	EXPECT_EQ(0b1000000, (GetFecHeaderOfNthFecPacket(fecPackets, 5)[18] & (0x7f))); // Mask [0-6]

	EXPECT_EQ(kSeqNum, get2(GetFecHeaderOfNthFecPacket(fecPackets, 6), 16)); // SN base_0
	EXPECT_EQ(0b1110000, (GetFecHeaderOfNthFecPacket(fecPackets, 6)[18] & (0x7f))); // Mask [0-6]
}

TEST_F(TestFecProbeGenerator, ShouldProduceAtLeastOnePacketWhenFecRateIsLowAndIsForcedTo)
{
	history.push_back(rtpPacketGenerator.Create(0));
	history.push_back(rtpPacketGenerator.Create(1));
	history.push_back(rtpPacketGenerator.Create(2));
	auto fecPackets = fecGenerator.PrepareFecProbePackets(0, true);

	EXPECT_EQ(1, fecPackets.size());
}
