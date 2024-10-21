#include "TestCommon.h"
#include "h264/H26xNal.h"
#include "h264/h264.h"

TEST(TestH26xSPS, TestParseSPS1)
{
	const std::vector<BYTE> spsData = {
		0x42, 0xc0, 0x16, 0xa6, 0x11, 0x05, 0x07, 0xe9,
		0xb2, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00,
		0x03, 0x00, 0x64, 0x1e, 0x2c, 0x5c, 0x23, 0x00
	};


	H264SeqParameterSet spsParser;

	spsParser.Decode(spsData.data(), spsData.size());
	// spsParser.Dump();

	EXPECT_EQ(66, spsParser.profile_idc);
	EXPECT_EQ(1, spsParser.constraint_set0_flag);
	EXPECT_EQ(1, spsParser.constraint_set1_flag);
	EXPECT_EQ(0, spsParser.constraint_set2_flag);
	EXPECT_EQ(0, spsParser.reserved_zero_5bits);
	EXPECT_EQ(22, spsParser.level_idc);
	EXPECT_EQ(0, spsParser.seq_parameter_set_id);
	EXPECT_EQ(1, spsParser.log2_max_frame_num_minus4);
	EXPECT_EQ(2, spsParser.pic_order_cnt_type);
	EXPECT_EQ(16, spsParser.num_ref_frames);
	EXPECT_EQ(0, spsParser.gaps_in_frame_num_value_allowed_flag);
	EXPECT_EQ(19, spsParser.pic_width_in_mbs_minus1);
	EXPECT_EQ(14, spsParser.pic_height_in_map_units_minus1);
	EXPECT_EQ(1, spsParser.frame_mbs_only_flag);
	EXPECT_EQ(1, spsParser.direct_8x8_inference_flag);
	EXPECT_EQ(0, spsParser.frame_cropping_flag);

	// vui parameters
	EXPECT_EQ(1,spsParser.vui_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.aspect_ratio_info_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.overscan_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.video_signal_type_present_flag);
	EXPECT_EQ(5, spsParser.vuiParams.video_format);
	EXPECT_EQ(1, spsParser.vuiParams.video_full_range_flag);
	EXPECT_EQ(0, spsParser.vuiParams.colour_description_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.chroma_loc_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.timing_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.num_units_in_tick);
	EXPECT_EQ(50, spsParser.vuiParams.time_scale);
	EXPECT_EQ(0, spsParser.vuiParams.fixed_frame_rate_flag);
	EXPECT_EQ(0, spsParser.vuiParams.nal_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.vcl_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.pic_struct_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.bitstream_restriction_flag);
	EXPECT_EQ(1, spsParser.vuiParams.motion_vectors_over_pic_boundaries_flag);
	EXPECT_EQ(0, spsParser.vuiParams.max_bytes_per_pic_denom);
	EXPECT_EQ(0, spsParser.vuiParams.max_bits_per_mb_denom);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_horizontal);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_vertical);
	EXPECT_EQ(0, spsParser.vuiParams.max_num_reorder_frames);
	EXPECT_EQ(16, spsParser.vuiParams.max_dec_frame_buffering);

}

TEST(TestH26xSPS, TestParseSPS2)
{
	const std::vector<BYTE> spsData = {
		0x64, 0x00, 0x33, 0xac, 0x72, 0x84, 0x40, 0x78,
		0x02, 0x27, 0xe5, 0xc0, 0x44, 0x00, 0x00, 0x03,
		0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xf0, 0x3c,
		0x60, 0xc6, 0x11, 0x80 
	};


	H264SeqParameterSet spsParser;

	spsParser.Decode(spsData.data(), spsData.size());
	// spsParser.Dump();

	EXPECT_EQ(100, spsParser.profile_idc);
	EXPECT_EQ(0, spsParser.constraint_set0_flag);
	EXPECT_EQ(0, spsParser.constraint_set1_flag);
	EXPECT_EQ(0, spsParser.constraint_set2_flag);
	EXPECT_EQ(0, spsParser.reserved_zero_5bits);
	EXPECT_EQ(51, spsParser.level_idc);
	EXPECT_EQ(0, spsParser.seq_parameter_set_id);
	EXPECT_EQ(1, spsParser.chroma_format_idc);
	EXPECT_EQ(2, spsParser.log2_max_frame_num_minus4);
	EXPECT_EQ(0, spsParser.pic_order_cnt_type);
	EXPECT_EQ(4, spsParser.log2_max_pic_order_cnt_lsb_minus4);
	EXPECT_EQ(16, spsParser.num_ref_frames);
	EXPECT_EQ(0, spsParser.gaps_in_frame_num_value_allowed_flag);
	EXPECT_EQ(119, spsParser.pic_width_in_mbs_minus1);
	EXPECT_EQ(67, spsParser.pic_height_in_map_units_minus1);
	EXPECT_EQ(1, spsParser.frame_mbs_only_flag);
	EXPECT_EQ(1, spsParser.direct_8x8_inference_flag);
	EXPECT_EQ(1, spsParser.frame_cropping_flag);
	EXPECT_EQ(0, spsParser.frame_crop_left_offset);
	EXPECT_EQ(0, spsParser.frame_crop_right_offset);
	EXPECT_EQ(0, spsParser.frame_crop_top_offset);
	EXPECT_EQ(4, spsParser.frame_crop_bottom_offset);

  	// vui_parameters()
	EXPECT_EQ(1, spsParser.vui_parameters_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.aspect_ratio_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.aspect_ratio_idc);
	EXPECT_EQ(0, spsParser.vuiParams.overscan_info_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.video_signal_type_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.chroma_loc_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.timing_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.num_units_in_tick);
	EXPECT_EQ(60, spsParser.vuiParams.time_scale);
	EXPECT_EQ(0, spsParser.vuiParams.fixed_frame_rate_flag);
	EXPECT_EQ(0, spsParser.vuiParams.nal_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.vcl_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.pic_struct_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.bitstream_restriction_flag);
	EXPECT_EQ(1, spsParser.vuiParams.motion_vectors_over_pic_boundaries_flag);
	EXPECT_EQ(0, spsParser.vuiParams.max_bytes_per_pic_denom);
	EXPECT_EQ(0, spsParser.vuiParams.max_bits_per_mb_denom);
	EXPECT_EQ(11, spsParser.vuiParams.log2_max_mv_length_horizontal);
	EXPECT_EQ(11, spsParser.vuiParams.log2_max_mv_length_vertical);
	EXPECT_EQ(2, spsParser.vuiParams.max_num_reorder_frames);
	EXPECT_EQ(16, spsParser.vuiParams.max_dec_frame_buffering);

}

TEST(TestH26xSPS, TestParseSPSVUI1)
{
	const std::vector<BYTE> spsData = {
		0x36, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
        0x0c, 0x83, 0xc5, 0x8b, 0x84, 0x60 
	};

	RbspReader reader(spsData.data(), spsData.size());
	RbspBitReader r(reader);
	H264SeqParameterSet spsParser;

	bool res = spsParser.DecodeVuiParameters(r);
	EXPECT_EQ(true, res);

  	// vui_parameters()
	EXPECT_EQ(0, spsParser.vuiParams.aspect_ratio_info_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.overscan_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.video_signal_type_present_flag);
	EXPECT_EQ(5, spsParser.vuiParams.video_format);
	EXPECT_EQ(1, spsParser.vuiParams.video_full_range_flag);
	EXPECT_EQ(0, spsParser.vuiParams.colour_description_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.chroma_loc_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.timing_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.num_units_in_tick);
	EXPECT_EQ(50, spsParser.vuiParams.time_scale);
	EXPECT_EQ(0, spsParser.vuiParams.fixed_frame_rate_flag);
	EXPECT_EQ(0, spsParser.vuiParams.nal_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.vcl_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.pic_struct_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.bitstream_restriction_flag);
	EXPECT_EQ(1, spsParser.vuiParams.motion_vectors_over_pic_boundaries_flag);
	EXPECT_EQ(0, spsParser.vuiParams.max_bytes_per_pic_denom);
	EXPECT_EQ(0, spsParser.vuiParams.max_bits_per_mb_denom);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_horizontal);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_vertical);
	EXPECT_EQ(0, spsParser.vuiParams.max_num_reorder_frames);
	EXPECT_EQ(16, spsParser.vuiParams.max_dec_frame_buffering);

}

TEST(TestH26xSPS, TestParseSPSVUI2)
{
	const std::vector<BYTE> spsData = {
		0x37, 0x06, 0x06, 0x06, 0x40, 0x00, 0x00, 0x00,
     	0x40, 0x00, 0x00, 0x0c, 0x83, 0xc5, 0x8b, 0x84,
     	0x60, 0x00

	};

	RbspReader reader(spsData.data(), spsData.size());
	RbspBitReader r(reader);
	H264SeqParameterSet spsParser;

	bool res = spsParser.DecodeVuiParameters(r);
	EXPECT_EQ(true, res);

  	// vui_parameters()
	EXPECT_EQ(0, spsParser.vuiParams.aspect_ratio_info_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.overscan_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.video_signal_type_present_flag);
	EXPECT_EQ(5, spsParser.vuiParams.video_format);
	EXPECT_EQ(1, spsParser.vuiParams.video_full_range_flag);
	EXPECT_EQ(1, spsParser.vuiParams.colour_description_present_flag);
	EXPECT_EQ(6, spsParser.vuiParams.colour_primaries);
	EXPECT_EQ(6, spsParser.vuiParams.transfer_characteristics);
	EXPECT_EQ(6, spsParser.vuiParams.matrix_coefficients);
	EXPECT_EQ(0, spsParser.vuiParams.chroma_loc_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.timing_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.num_units_in_tick);
	EXPECT_EQ(50, spsParser.vuiParams.time_scale);
	EXPECT_EQ(0, spsParser.vuiParams.fixed_frame_rate_flag);
	EXPECT_EQ(0, spsParser.vuiParams.nal_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.vcl_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.pic_struct_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.bitstream_restriction_flag);
	EXPECT_EQ(1, spsParser.vuiParams.motion_vectors_over_pic_boundaries_flag);
	EXPECT_EQ(0, spsParser.vuiParams.max_bytes_per_pic_denom);
	EXPECT_EQ(0, spsParser.vuiParams.max_bits_per_mb_denom);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_horizontal);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_vertical);
	EXPECT_EQ(0, spsParser.vuiParams.max_num_reorder_frames);
	EXPECT_EQ(16, spsParser.vuiParams.max_dec_frame_buffering);

}

TEST(TestH26xSPS, TestParseSPSVUI3)
{
	const std::vector<BYTE> spsData = {
	  0x37, 0x01, 0x01, 0x01, 0x40, 0x00, 0x00, 0x00,
      0x40, 0x00, 0x00, 0x0c, 0x83, 0xc5, 0x8b, 0x84,
      0x60, 0x00

	};

	RbspReader reader(spsData.data(), spsData.size());
	RbspBitReader r(reader);
	H264SeqParameterSet spsParser;

	bool res = spsParser.DecodeVuiParameters(r);
	EXPECT_EQ(true, res);

  	// vui_parameters()
	EXPECT_EQ(0, spsParser.vuiParams.aspect_ratio_info_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.overscan_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.video_signal_type_present_flag);
	EXPECT_EQ(5, spsParser.vuiParams.video_format);
	EXPECT_EQ(1, spsParser.vuiParams.video_full_range_flag);
	EXPECT_EQ(1, spsParser.vuiParams.colour_description_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.colour_primaries);
	EXPECT_EQ(1, spsParser.vuiParams.transfer_characteristics);
	EXPECT_EQ(1, spsParser.vuiParams.matrix_coefficients);
	EXPECT_EQ(0, spsParser.vuiParams.chroma_loc_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.timing_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.num_units_in_tick);
	EXPECT_EQ(50, spsParser.vuiParams.time_scale);
	EXPECT_EQ(0, spsParser.vuiParams.fixed_frame_rate_flag);
	EXPECT_EQ(0, spsParser.vuiParams.nal_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.vcl_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.pic_struct_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.bitstream_restriction_flag);
	EXPECT_EQ(1, spsParser.vuiParams.motion_vectors_over_pic_boundaries_flag);
	EXPECT_EQ(0, spsParser.vuiParams.max_bytes_per_pic_denom);
	EXPECT_EQ(0, spsParser.vuiParams.max_bits_per_mb_denom);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_horizontal);
	EXPECT_EQ(10, spsParser.vuiParams.log2_max_mv_length_vertical);
	EXPECT_EQ(0, spsParser.vuiParams.max_num_reorder_frames);
	EXPECT_EQ(16, spsParser.vuiParams.max_dec_frame_buffering);

}

TEST(TestH26xSPS, TestParseSPSVUI4)
{
	const std::vector<BYTE> spsData = {
	  0x37, 0x05, 0x06, 0x05, 0x40, 0x39, 0xcc, 0x66,
      0xce, 0xe6, 0xb2, 0x80, 0x23, 0x68, 0x50, 0x9a,
      0x80
	};

	RbspReader reader(spsData.data(), spsData.size());
	RbspBitReader r(reader);
	H264SeqParameterSet spsParser;

	bool res = spsParser.DecodeVuiParameters(r);
	EXPECT_EQ(true, res);

  	// vui_parameters()
	EXPECT_EQ(0, spsParser.vuiParams.aspect_ratio_info_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.overscan_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.video_signal_type_present_flag);
	EXPECT_EQ(5, spsParser.vuiParams.video_format);
	EXPECT_EQ(1, spsParser.vuiParams.video_full_range_flag);
	EXPECT_EQ(1, spsParser.vuiParams.colour_description_present_flag);
	EXPECT_EQ(5, spsParser.vuiParams.colour_primaries);
	EXPECT_EQ(6, spsParser.vuiParams.transfer_characteristics);
	EXPECT_EQ(5, spsParser.vuiParams.matrix_coefficients);
	EXPECT_EQ(0, spsParser.vuiParams.chroma_loc_info_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.timing_info_present_flag);
	EXPECT_EQ(15151515, spsParser.vuiParams.num_units_in_tick);
	EXPECT_EQ(1000000000, spsParser.vuiParams.time_scale);
	EXPECT_EQ(1, spsParser.vuiParams.fixed_frame_rate_flag);
	EXPECT_EQ(0, spsParser.vuiParams.nal_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.vcl_hrd_parameters_present_flag);
	EXPECT_EQ(0, spsParser.vuiParams.pic_struct_present_flag);
	EXPECT_EQ(1, spsParser.vuiParams.bitstream_restriction_flag);
	EXPECT_EQ(1, spsParser.vuiParams.motion_vectors_over_pic_boundaries_flag);
	EXPECT_EQ(2, spsParser.vuiParams.max_bytes_per_pic_denom);
	EXPECT_EQ(1, spsParser.vuiParams.max_bits_per_mb_denom);
	EXPECT_EQ(9, spsParser.vuiParams.log2_max_mv_length_horizontal);
	EXPECT_EQ(8, spsParser.vuiParams.log2_max_mv_length_vertical);
	EXPECT_EQ(0, spsParser.vuiParams.max_num_reorder_frames);
	EXPECT_EQ(1, spsParser.vuiParams.max_dec_frame_buffering);

}

TEST(TestH26xSPS, TestParseSPSHRD)
{
	const std::vector<BYTE> spsData = {
	 0x9a, 0x80, 0x00, 0x4c, 0x4b, 0x40, 0x00, 0x26,
     0x25, 0xaf, 0xd6, 0xb8, 0x1e, 0x30, 0x62, 0x24
	};

	RbspReader reader(spsData.data(), spsData.size());
	RbspBitReader r(reader);
	H264SeqParameterSet spsParser;

	// for the unit test it doesn't matter if we pick nal_hrd_parameters or vcl_hrd_parameters
	bool res = spsParser.ParseHRDParams(r, spsParser.vuiParams.nal_hrd_parameters);
	EXPECT_EQ(true, res);

  	// hrd_parameters()
	EXPECT_EQ(0, spsParser.vuiParams.nal_hrd_parameters.cpb_cnt_minus1);
	EXPECT_EQ(3, spsParser.vuiParams.nal_hrd_parameters.bit_rate_scale);
	EXPECT_EQ(5, spsParser.vuiParams.nal_hrd_parameters.cpb_size_scale);
	EXPECT_THAT(spsParser.vuiParams.nal_hrd_parameters.bit_rate_value_minus1,
				::testing::ElementsAreArray({78124}));
	EXPECT_THAT(spsParser.vuiParams.nal_hrd_parameters.cpb_size_value_minus1,
				::testing::ElementsAreArray({78124}));
	EXPECT_THAT(spsParser.vuiParams.nal_hrd_parameters.cbr_flag, ::testing::ElementsAreArray({0}));
	EXPECT_EQ(31, spsParser.vuiParams.nal_hrd_parameters.initial_cpb_removal_delay_length_minus1);
	EXPECT_EQ(21, spsParser.vuiParams.nal_hrd_parameters.cpb_removal_delay_length_minus1);
	EXPECT_EQ(21, spsParser.vuiParams.nal_hrd_parameters.dpb_output_delay_length_minus1);
	EXPECT_EQ(24, spsParser.vuiParams.nal_hrd_parameters.time_offset_length);

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