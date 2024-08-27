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
	EXPECT_TRUE(header.field.N);
	EXPECT_TRUE(header.field.Z);
	EXPECT_TRUE(header.field.W);

	// We only parse the first OBU. The size field would not present when only one element in the packet
	uint32_t obuSize = reader.GetLeft();
	if (header.field.W != 1)
	{
		obuSize = reader.DecodeLev128();
	}
	
	ASSERT_TRUE(obuHeader.Parse(reader));
	Log("%d\n", reader.Mark());
	obuHeader.Dump();
	EXPECT_EQ(obuHeader.type, ObuType::ObuSequenceHeader);
	::Dump(reader.PeekData(),16);
	ASSERT_TRUE(sho.Parse(reader));
	sho.Dump();
}

/*
mark:2
24 1
[0000] [0x10   0x08   0x04   0x00   0x00   0x00   0x04   0x00   ........]
[0008] [0x00   0x00   0x63   0x40   0x00   0x08   0x42   0xab   ..c@..B.]
[0010] [0xbf   0xc3   0x70   0x08   0x74   0x40   0x40   0x40   ..p.t@@@]
[0018] [0x49   0x00   0x00   0x00   0x00   0x00   0x00   0x00   I.......]
[0020] [0x00   0x00   0x00   0x00   0x00   0x00   0x00   0x00   ........]
[0028] [0x00   0x00   0x00   0x00   0x00   0x00   0x00   0x00   ........]
[0030] [0x00   0x00   0x00   0x00   0x00   0x00   0x00   0x00   ........]
[0038] [0x00   0x00   0x00   0x00   0x00   0x00   0x00   0x00   ........]
[SequenceHeaderObu
seq_profile=0
still_picture=0
reduced_still_picture_header=0
timing_info_present_flag=1
decoder_model_info_present_flag=0
initial_display_delay_present_flag=0
operating_points_cnt_minus_1=0
operating_point_idc[0]=16
seq_level_idx[0]=16
seq_tier[0]=1
decoder_model_present_for_this_op[0]=0
max_frame_width_minus_1=55
max_frame_height_minus_1=62
frame_id_numbers_present_flag=0
delta_frame_id_length_minus_2=0
additional_frame_id_length_minus_1=0
use_128x128_superblock=0
enable_filter_intra=0
enable_intra_edge_filter=1
enable_interintra_compound=1
enable_masked_compound=0
enable_warped_motion=1
enable_dual_filter=1
enable_order_hint=1
enable_jnt_comp=0
enable_ref_frame_mvs=0
seq_force_screen_content_tools=0
seq_force_integer_mv=2
enable_superres=0
enable_cdef=1
enable_restoration=0
film_grain_params_present=115
seq_choose_screen_content_tools=0
seq_choose_integer_mv=0
order_hint_bits_minus_1=0
num_units_in_display_tick=1
time_scale=24
equal_picture_interval=1
num_ticks_per_picture_minus_1=3
buffer_delay_length_minus_1=160
num_units_in_decoding_tick=29342
buffer_removal_time_length_minus_1=24
frame_presentation_time_length_minus_1=115
/]

*/