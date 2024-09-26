#include "TestCommon.h"
#include "h264/H26xNal.h"
#include "h264/h264.h"

TEST(TestH26xSPS, TestParseSPS1)
{
	const std::vector<BYTE> spsData = {0x42, 0xc0, 0x16, 0xa6, 0x11, 0x05, 0x07, 0xe9,
      0xb2, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00,
      0x03, 0x00, 0x64, 0x1e, 0x2c, 0x5c, 0x23, 0x00};


	H264SeqParameterSet spsParser;

	spsParser.Decode(spsData.data(), spsData.size());
	// spsParser.Dump();
    auto spsInfo = spsParser.GetSPSData();

    EXPECT_EQ(66, spsInfo.profile_idc);
    EXPECT_EQ(1, spsInfo.constraint_set0_flag);
    EXPECT_EQ(1, spsInfo.constraint_set1_flag);
    EXPECT_EQ(0, spsInfo.constraint_set2_flag);
    EXPECT_EQ(0, spsInfo.reserved_zero_5bits);
    EXPECT_EQ(22, spsInfo.level_idc);
    EXPECT_EQ(0, spsInfo.seq_parameter_set_id);
    EXPECT_EQ(1, spsInfo.log2_max_frame_num_minus4);
    EXPECT_EQ(2, spsInfo.pic_order_cnt_type);
    EXPECT_EQ(16, spsInfo.num_ref_frames);
    EXPECT_EQ(0, spsInfo.gaps_in_frame_num_value_allowed_flag);
    EXPECT_EQ(19, spsInfo.pic_width_in_mbs_minus1);
    EXPECT_EQ(14, spsInfo.pic_height_in_map_units_minus1);
    EXPECT_EQ(1, spsInfo.frame_mbs_only_flag);
    EXPECT_EQ(1, spsInfo.direct_8x8_inference_flag);
    EXPECT_EQ(0, spsInfo.frame_cropping_flag);

}

TEST(TestH26xSPS, TestParseSPS2)
{
	const std::vector<BYTE> spsData = {0x64, 0x00, 0x33, 0xac, 0x72, 0x84, 0x40, 0x78,
      0x02, 0x27, 0xe5, 0xc0, 0x44, 0x00, 0x00, 0x03,
      0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xf0, 0x3c,
      0x60, 0xc6, 0x11, 0x80};


	H264SeqParameterSet spsParser;

	spsParser.Decode(spsData.data(), spsData.size());
	// spsParser.Dump();
    auto spsInfo = spsParser.GetSPSData();

	EXPECT_EQ(100, spsInfo.profile_idc);
    EXPECT_EQ(0, spsInfo.constraint_set0_flag);
    EXPECT_EQ(0, spsInfo.constraint_set1_flag);
    EXPECT_EQ(0, spsInfo.constraint_set2_flag);
    EXPECT_EQ(0, spsInfo.reserved_zero_5bits);
    EXPECT_EQ(51, spsInfo.level_idc);
    EXPECT_EQ(0, spsInfo.seq_parameter_set_id);
    EXPECT_EQ(1, spsInfo.chroma_format_idc);
    EXPECT_EQ(2, spsInfo.log2_max_frame_num_minus4);
    EXPECT_EQ(0, spsInfo.pic_order_cnt_type);
    EXPECT_EQ(4, spsInfo.log2_max_pic_order_cnt_lsb_minus4);
    EXPECT_EQ(16, spsInfo.num_ref_frames);
    EXPECT_EQ(0, spsInfo.gaps_in_frame_num_value_allowed_flag);
    EXPECT_EQ(119, spsInfo.pic_width_in_mbs_minus1);
    EXPECT_EQ(67, spsInfo.pic_height_in_map_units_minus1);
    EXPECT_EQ(1, spsInfo.frame_mbs_only_flag);
    EXPECT_EQ(1, spsInfo.direct_8x8_inference_flag);
    EXPECT_EQ(1, spsInfo.frame_cropping_flag);
    EXPECT_EQ(0, spsInfo.frame_crop_left_offset);
    EXPECT_EQ(0, spsInfo.frame_crop_right_offset);
    EXPECT_EQ(0, spsInfo.frame_crop_top_offset);
    EXPECT_EQ(4, spsInfo.frame_crop_bottom_offset);


}

// more potential test vectors, need to compute expected values

// extracted from ffmpeg 
// const std::vector<BYTE> spsData = {0x67, 0x64, 0x00, 
	// 									  0x1E, 0xAC, 0xB2, 0x01, 0x40, 0x7B, 0x60, 
	// 									  0x22, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 
	// 									  0x00, 0x03, 0x00, 0x64, 0x1E, 0x2C, 0x5C, 
	// 									  0x90 };


// captured in the media server 
// const std::vector<BYTE> spsData = {0xf4, 0x00, 0x1e, 0x91, 0x9b, 0x28, 0x14, 0x07, 0xb6, 0x02, 0x20, 0x00, 0x00,
//        0x03, 0x00, 0x20, 0x00, 0x00, 0x07, 0x81, 0xe2, 0xc5, 0xb2, 0xc0};