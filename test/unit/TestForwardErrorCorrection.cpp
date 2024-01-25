#include "ForwardErrorCorrection.h"
#include "RtpTestCommon.h"
#include <gtest/gtest.h>
#include "tools.h"

constexpr BYTE kFixedHeaderSize = 12;

constexpr DWORD kFecHeaderMask15Size = 20;
constexpr DWORD kFecHeaderMask46Size = 24;
constexpr DWORD kFecHeaderMask109Size = 32;

constexpr WORD kMaxFecRate = 255;

class TestForwardErrorCorrection : public testing::Test
{
public:
	TestForwardErrorCorrection()
	{
		Logger::EnableDebug(true);
	}

	RTPMap rtpMap;
	RTPMap extMap;
	RTPPacketGenerator rtpPacketGenerator{rtpMap, extMap};
	std::vector<RTPPacket::shared> mediaPackets;
	ForwardErrorCorrection fec;
};

BYTE getFecHeaderKbit0(const RTPPacket::shared rtp)
{
	const BYTE* fecHeader = rtp->GetMediaData();
	return (fecHeader[18] & (1 << 7)) >> 7;
}

BYTE getFecHeaderKbit1(const RTPPacket::shared rtp)
{
	const BYTE* fecHeader = rtp->GetMediaData();
	return (fecHeader[20] & (1 << 7)) >> 7;
}

BYTE getFecHeaderKbit2(const RTPPacket::shared rtp)
{
	const BYTE* fecHeader = rtp->GetMediaData();
	return (fecHeader[24] & (1 << 7)) >> 7;
}

BYTE getFecHeaderSize(const RTPPacket::shared rtp)
{
	if (getFecHeaderKbit0(rtp)) return kFecHeaderMask15Size;
	else if (getFecHeaderKbit1(rtp)) return kFecHeaderMask46Size;
	else if (getFecHeaderKbit2(rtp)) return kFecHeaderMask109Size;

	EXPECT_TRUE(!"Unsupported K-bits setting");
	return 0;
}

void CheckBytes(const std::vector<BYTE>& actual, const std::vector<BYTE>& expected)
{
	ASSERT_EQ(expected.size(), actual.size()) << "Vectors x and y are of unequal length";

	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(expected[i], actual[i]) << "Vectors x and y differ at index " << i;
	}
}

std::vector<BYTE> GetRepairCode(RTPPacket::shared rtp)
{
	EXPECT_GT(rtp->GetMediaLength(), getFecHeaderSize(rtp));
	return {rtp->GetMediaData() + getFecHeaderSize(rtp), rtp->GetMediaData() + rtp->GetMediaLength()};
}

std::vector<BYTE> GetPayload(RTPPacket::shared rtp)
{
	return {rtp->GetMediaData(), rtp->GetMediaData() + rtp->GetMediaLength()};
}

TEST_F(TestForwardErrorCorrection, FecPacketForSingleMediaPacketShouldJustCopyPayload)
{
	mediaPackets.push_back(rtpPacketGenerator.Create());

	auto fecPacket = fec.EncodeFec(mediaPackets, extMap);

	ASSERT_TRUE(fecPacket);
	// Fec packet's payload after fec header should match media packet
	CheckBytes(GetPayload(mediaPackets[0]), GetRepairCode(fecPacket));
}

TEST_F(TestForwardErrorCorrection, FecPacketForOddNumberOfSameMediaPacketShouldJustCopyPayload)
{
	mediaPackets.push_back(rtpPacketGenerator.Create(0));
	mediaPackets.push_back(rtpPacketGenerator.Create(1));
	mediaPackets.push_back(rtpPacketGenerator.Create(2));

	auto fecPacket = fec.EncodeFec(mediaPackets, extMap);

	ASSERT_TRUE(fecPacket);
	CheckBytes(GetPayload(mediaPackets[0]), GetRepairCode(fecPacket));
	const BYTE* fecHeader = fecPacket->GetMediaData();
	EXPECT_EQ(0, (fecHeader[0] & (1 << 7)) >> 7 ); // R
	EXPECT_EQ(0, (fecHeader[0] & (1 << 6)) >> 6 ); // F
	EXPECT_EQ(0, (fecHeader[0] & (1 << 5)) >> 5 ); // P
	EXPECT_EQ(0, (fecHeader[0] & (1 << 4)) >> 4 ); // X
	EXPECT_EQ(0, (fecHeader[0] & (0x0f))); // CC
	EXPECT_EQ(0, (fecHeader[1] & (1 << 7)) >> 7 ); // M
	EXPECT_EQ(kPayloadType, (fecHeader[1] & (0x7f))); // PT recovery
	EXPECT_EQ(mediaPackets.front()->GetMediaLength(), get2(fecHeader, 2)); // length recovery
	EXPECT_EQ(kTimeStamp, get4(fecHeader, 4)); // TS recovery
	EXPECT_EQ(1, fecHeader[8]); // SSRCCount
	EXPECT_EQ(0, get3(fecHeader, 9)); // reserved
	EXPECT_EQ(kSsrc, get4(fecHeader, 12)); // SSRC_0
	EXPECT_EQ(kSeqNum, get2(fecHeader, 16)); // SN base_0
	EXPECT_EQ(1, (fecHeader[18] & (1 << 7)) >> 7 ); // k-bit 0
	EXPECT_EQ(0b1110000, (fecHeader[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(0, fecHeader[19]); // Mask [7-14]
}

TEST_F(TestForwardErrorCorrection, FecPacketFor17SamePacketsShouldUse46LongMask)
{
	for(DWORD i = 0; i < 17; ++i)
		mediaPackets.push_back(rtpPacketGenerator.Create(i));

	auto fecPacket = fec.EncodeFec(mediaPackets, extMap);

	ASSERT_TRUE(fecPacket);
	CheckBytes(GetPayload(mediaPackets[0]), GetRepairCode(fecPacket));
	const BYTE* fecHeader = fecPacket->GetMediaData();
	EXPECT_EQ(0, (fecHeader[0] & (1 << 7)) >> 7 ); // R
	EXPECT_EQ(0, (fecHeader[0] & (1 << 6)) >> 6 ); // F
	EXPECT_EQ(0, (fecHeader[0] & (1 << 5)) >> 5 ); // P
	EXPECT_EQ(0, (fecHeader[0] & (1 << 4)) >> 4 ); // X
	EXPECT_EQ(0, (fecHeader[0] & (0x0f))); // CC
	EXPECT_EQ(0, (fecHeader[1] & (1 << 7)) >> 7 ); // M
	EXPECT_EQ(kPayloadType, (fecHeader[1] & (0x7f))); // PT recovery
	EXPECT_EQ(mediaPackets.front()->GetMediaLength(), get2(fecHeader, 2)); // length recovery
	EXPECT_EQ(kTimeStamp, get4(fecHeader, 4)); // TS recovery
	EXPECT_EQ(1, fecHeader[8]); // SSRCCount
	EXPECT_EQ(0, get3(fecHeader, 9)); // reserved
	EXPECT_EQ(kSsrc, get4(fecHeader, 12)); // SSRC_0
	EXPECT_EQ(kSeqNum, get2(fecHeader, 16)); // SN base_0
	EXPECT_EQ(0, (fecHeader[18] & (1 << 7)) >> 7 ); // k-bit 0
	EXPECT_EQ(0b1111111, (fecHeader[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(0xFF, fecHeader[19]); // Mask [7-14]
	EXPECT_EQ(1, (fecHeader[20] & (1 << 7)) >> 7 ); // k-bit 0
	EXPECT_EQ(0b1100000, (fecHeader[20] & (0x7f))); // Mask [15-21]
	EXPECT_EQ(0, fecHeader[21]); // Mask [22-29]
	EXPECT_EQ(0, fecHeader[22]); // Mask [30-37]
	EXPECT_EQ(0, fecHeader[23]); // Mask [38-45]
}

TEST_F(TestForwardErrorCorrection, DuplicateMediaPacketsShouldBeSkippedWhenEncodingFec)
{
	mediaPackets.push_back(rtpPacketGenerator.Create());
	mediaPackets.push_back(rtpPacketGenerator.Create());

	auto fecPacket = fec.EncodeFec(mediaPackets, extMap);

	ASSERT_TRUE(fecPacket);
	// There are 2 same packets on the input, if one was skipped, then fec payload
	// after fec header will be the copy of the payload of the media packet
	CheckBytes(GetPayload(mediaPackets[0]), GetRepairCode(fecPacket));
}

TEST_F(TestForwardErrorCorrection, SeqNumOverflowInInputMediaPacketsShouldBeHandledCorrectly)
{
	mediaPackets.push_back(rtpPacketGenerator.Create(0xFFFF - kSeqNum - 1)); //65534
	mediaPackets.push_back(rtpPacketGenerator.Create(0xFFFF - kSeqNum)); //65535
	mediaPackets.push_back(rtpPacketGenerator.Create(0xFFFF - kSeqNum + 1)); // 0
	mediaPackets.push_back(rtpPacketGenerator.Create(0xFFFF - kSeqNum + 2)); // 1
	mediaPackets.push_back(rtpPacketGenerator.Create(0xFFFF - kSeqNum + 3)); // 2

	auto fecPacket = fec.EncodeFec(mediaPackets, extMap);

	ASSERT_TRUE(fecPacket);
	// There are packets with the same payload to be protected. Without seq num overload handling
	// only 2 media packets payloads would be xored, which would produce all 0 after fec header in fec
	// packet's payload. If the overload is handled correctly, 5 media packets payload will be xored,
	// so fec payload after fec header will be the copy of the payload of the media packet
	CheckBytes(GetPayload(mediaPackets[0]), GetRepairCode(fecPacket));
}

TEST_F(TestForwardErrorCorrection, FecPacketFor47SamePacketsShouldUse109LongMask)
{
	fec.SetMaxAllowedMaskSize(109);

	for(DWORD i = 0; i < 47; ++i)
		mediaPackets.push_back(rtpPacketGenerator.Create(i));

	auto fecPacket = fec.EncodeFec(mediaPackets, extMap);

	ASSERT_TRUE(fecPacket);
	CheckBytes(GetPayload(mediaPackets[0]), GetRepairCode(fecPacket));
	const BYTE* fecHeader = fecPacket->GetMediaData();
	EXPECT_EQ(0, (fecHeader[0] & (1 << 7)) >> 7 ); // R
	EXPECT_EQ(0, (fecHeader[0] & (1 << 6)) >> 6 ); // F
	EXPECT_EQ(0, (fecHeader[0] & (1 << 5)) >> 5 ); // P
	EXPECT_EQ(0, (fecHeader[0] & (1 << 4)) >> 4 ); // X
	EXPECT_EQ(0, (fecHeader[0] & (0x0f))); // CC
	EXPECT_EQ(0, (fecHeader[1] & (1 << 7)) >> 7 ); // M
	EXPECT_EQ(kPayloadType, (fecHeader[1] & (0x7f))); // PT recovery
	EXPECT_EQ(mediaPackets.front()->GetMediaLength(), get2(fecHeader, 2)); // length recovery
	EXPECT_EQ(kTimeStamp, get4(fecHeader, 4)); // TS recovery
	EXPECT_EQ(1, fecHeader[8]); // SSRCCount
	EXPECT_EQ(0, get3(fecHeader, 9)); // reserved
	EXPECT_EQ(kSsrc, get4(fecHeader, 12)); // SSRC_0
	EXPECT_EQ(kSeqNum, get2(fecHeader, 16)); // SN base_0
	EXPECT_EQ(0, (fecHeader[18] & (1 << 7)) >> 7 ); // k-bit 0
	EXPECT_EQ(0b1111111, (fecHeader[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(0xFF, fecHeader[19]); // Mask [7-14]
	EXPECT_EQ(0, (fecHeader[20] & (1 << 7)) >> 7 ); // k-bit 1
	EXPECT_EQ(0b1111111, (fecHeader[20] & (0x7f))); // Mask [15-21]
	EXPECT_EQ(0xFF, fecHeader[21]); // Mask [22-29]
	EXPECT_EQ(0xFF, fecHeader[22]); // Mask [30-37]
	EXPECT_EQ(0xFF, fecHeader[23]); // Mask [38-45]
	EXPECT_EQ(1, (fecHeader[24] & (1 << 7)) >> 7 ); // k-bit 2
	EXPECT_EQ(0b1000000, (fecHeader[24] & (0x7f))); // Mask [46-52]
	EXPECT_EQ(0, fecHeader[25]); // Mask [53-60]
	EXPECT_EQ(0, fecHeader[26]); // Mask [61-68]
	EXPECT_EQ(0, fecHeader[27]); // Mask [69-76]
	EXPECT_EQ(0, fecHeader[28]); // Mask [77-84]
	EXPECT_EQ(0, fecHeader[29]); // Mask [85-92]
	EXPECT_EQ(0, fecHeader[30]); // Mask [93-100]
	EXPECT_EQ(0, fecHeader[31]); // Mask [101-108]
}

TEST_F(TestForwardErrorCorrection, FecPacketShouldHaveBitsCorrectlySetInMask)
{
	fec.SetMaxAllowedMaskSize(109);
	mediaPackets.push_back(rtpPacketGenerator.Create(0));
	mediaPackets.push_back(rtpPacketGenerator.Create(1));
	mediaPackets.push_back(rtpPacketGenerator.Create(10));
	mediaPackets.push_back(rtpPacketGenerator.Create(20));
	mediaPackets.push_back(rtpPacketGenerator.Create(46));
	mediaPackets.push_back(rtpPacketGenerator.Create(50));
	mediaPackets.push_back(rtpPacketGenerator.Create(90));
	mediaPackets.push_back(rtpPacketGenerator.Create(108));

	auto fecPacket = fec.EncodeFec(mediaPackets, extMap);

	ASSERT_TRUE(fecPacket);
	const BYTE* fecHeader = fecPacket->GetMediaData();
	EXPECT_EQ(0, (fecHeader[18] & (1 << 7)) >> 7 ); // k-bit 0
	EXPECT_EQ(0b1100000, (fecHeader[18] & (0x7f))); // Mask [0-6]
	EXPECT_EQ(0b00010000, fecHeader[19]); // Mask [7-14]
	EXPECT_EQ(0, (fecHeader[20] & (1 << 7)) >> 7 ); // k-bit 1
	EXPECT_EQ(0b0000010, (fecHeader[20] & (0x7f))); // Mask [15-21]
	EXPECT_EQ(0, fecHeader[21]); // Mask [22-29]
	EXPECT_EQ(0, fecHeader[22]); // Mask [30-37]
	EXPECT_EQ(0, fecHeader[23]); // Mask [38-45]
	EXPECT_EQ(1, (fecHeader[24] & (1 << 7)) >> 7 ); // k-bit 2
	EXPECT_EQ(0b1000100, (fecHeader[24] & (0x7f))); // Mask [46-52]
	EXPECT_EQ(0, fecHeader[25]); // Mask [53-60]
	EXPECT_EQ(0, fecHeader[26]); // Mask [61-68]
	EXPECT_EQ(0, fecHeader[27]); // Mask [69-76]
	EXPECT_EQ(0, fecHeader[28]); // Mask [77-84]
	EXPECT_EQ(0b00000100, fecHeader[29]); // Mask [85-92]
	EXPECT_EQ(0, fecHeader[30]); // Mask [93-100]
	EXPECT_EQ(0b00000001, fecHeader[31]); // Mask [101-108]
}
