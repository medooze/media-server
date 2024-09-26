#include "TestCommon.h"
#include "h264/H26xNal.h"
#include "h264/h264.h"

TEST(TestH26xPPS, TestParsePPS1)
{
	
	const std::vector<BYTE> ppsData = {0xc8, 0x42, 0x02, 0x32, 0xc8};


	H264PictureParameterSet ppsParser;

	ppsParser.Decode(ppsData.data(), ppsData.size());
	// ppsParser.Dump();

    EXPECT_EQ(0, ppsParser.pic_parameter_set_id);
    EXPECT_EQ(0, ppsParser.seq_parameter_set_id);
    EXPECT_EQ(0, ppsParser.entropy_coding_mode_flag);
    EXPECT_EQ(0, ppsParser.pic_order_present_flag);
    EXPECT_EQ(0, ppsParser.num_slice_groups_minus1);
    EXPECT_EQ(15, ppsParser.num_ref_idx_l0_active_minus1);
    EXPECT_EQ(0, ppsParser.num_ref_idx_l1_active_minus1);
    EXPECT_EQ(0, ppsParser.weighted_pred_flag);
    EXPECT_EQ(0, ppsParser.weighted_bipred_idc);
    EXPECT_EQ(-8, ppsParser.pic_init_qp_minus26);
    EXPECT_EQ(0, ppsParser.pic_init_qs_minus26);
    EXPECT_EQ(-2, ppsParser.chroma_qp_index_offset);
    EXPECT_EQ(1, ppsParser.deblocking_filter_control_present_flag);
    EXPECT_EQ(0, ppsParser.constrained_intra_pred_flag);
    EXPECT_EQ(0, ppsParser.redundant_pic_cnt_present_flag);


}

TEST(TestH26xPPS, TestParsePPS2)
{
	
	const std::vector<BYTE> ppsData = {0xe8, 0x43, 0x82, 0x92, 0xc8, 0xb0};


	H264PictureParameterSet ppsParser;

	ppsParser.Decode(ppsData.data(), ppsData.size());
	// ppsParser.Dump();

    EXPECT_EQ(0, ppsParser.pic_parameter_set_id);
    EXPECT_EQ(0, ppsParser.seq_parameter_set_id);
    EXPECT_EQ(1, ppsParser.entropy_coding_mode_flag);
    EXPECT_EQ(0, ppsParser.pic_order_present_flag);
    EXPECT_EQ(0, ppsParser.num_slice_groups_minus1);
    EXPECT_EQ(15, ppsParser.num_ref_idx_l0_active_minus1);
    EXPECT_EQ(0, ppsParser.num_ref_idx_l1_active_minus1);
    EXPECT_EQ(1, ppsParser.weighted_pred_flag);
    EXPECT_EQ(2, ppsParser.weighted_bipred_idc);
    EXPECT_EQ(10, ppsParser.pic_init_qp_minus26);
    EXPECT_EQ(0, ppsParser.pic_init_qs_minus26);
    EXPECT_EQ(-2, ppsParser.chroma_qp_index_offset);
    EXPECT_EQ(1, ppsParser.deblocking_filter_control_present_flag);
    EXPECT_EQ(0, ppsParser.constrained_intra_pred_flag);
    EXPECT_EQ(0, ppsParser.redundant_pic_cnt_present_flag);
    // We are not yet parsing these params? leaving it here as a comment for future reference
    // EXPECT_EQ(1, ppsParser.transform_8x8_mode_flag);
    // EXPECT_EQ(0, ppsParser.pic_scaling_matrix_present_flag);
    // EXPECT_EQ(-2, ppsParser.second_chroma_qp_index_offset);

	

}