#include "TestCommon.h"
#include "av1/AV1.h"
#include "av1/Obu.h"



TEST(TestAV1Bitstream, NetintWidthHeight)
{
	Logger::EnableDebug(true);

	const uint8_t rtpPayload[] = {
		0x10,   0x08,   0x04,   0x00,   0x00,   0x00,   0x04,   0x00,
		0x00,   0x00,   0x63,   0x40,   0x00,   0x08,   0x42,   0xab,
		0xbf,   0xc3,   0x70,   0x08,   0x74,   0x40,   0x40,   0x40,
		0x49,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,
		0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,
		0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,
		0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,
		0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,   0x00,
	};

	BufferReader reader(rtpPayload, sizeof(rtpPayload));

	RtpAv1AggreationHeader header;
	ObuHeader obuHeader;
	SequenceHeaderObu sho;

	ASSERT_TRUE(header.Parse(reader));
	EXPECT_FALSE(header.field.N);
	EXPECT_FALSE(header.field.Z);
	EXPECT_TRUE(header.field.W);

	// We only parse the first OBU. The size field would not present when only one element in the packet
	uint32_t obuSize = reader.GetLeft();
	if (header.field.W != 1)
	{
		obuSize = reader.DecodeLev128();
	}
	
	ASSERT_TRUE(obuHeader.Parse(reader));
	EXPECT_EQ(obuHeader.type, ObuType::ObuSequenceHeader);

	ASSERT_TRUE(sho.Parse(reader));
	EXPECT_EQ(sho.seq_profile,0);
	EXPECT_EQ(sho.max_frame_width_minus_1 + 1, 1920);
	EXPECT_EQ(sho.max_frame_height_minus_1 + 1, 1080);
}
