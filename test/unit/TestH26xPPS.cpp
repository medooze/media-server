#include "TestCommon.h"
#include "h264/H26xNal.h"
#include "h264/h264.h"

TEST(TestH26xPPS, TestParsePPS1)
{
	
	const std::vector<BYTE> ppsData = {0xc8, 0x42, 0x02, 0x32, 0xc8};


	H264PictureParameterSet ppsParser;

	ppsParser.Decode(ppsData.data(), ppsData.size());
	// ppsParser.Dump();

    auto ppsInfo = ppsParser.GetPPSData();

    EXPECT_EQ(0, ppsInfo.pic_parameter_set_id);
    EXPECT_EQ(0, ppsInfo.seq_parameter_set_id);
    EXPECT_EQ(0, ppsInfo.entropy_coding_mode_flag);
    EXPECT_EQ(0, ppsInfo.pic_order_present_flag);
    EXPECT_EQ(0, ppsInfo.num_slice_groups_minus1);
    EXPECT_EQ(15, ppsInfo.num_ref_idx_l0_active_minus1);
    EXPECT_EQ(0, ppsInfo.num_ref_idx_l1_active_minus1);
    EXPECT_EQ(0, ppsInfo.weighted_pred_flag);
    EXPECT_EQ(0, ppsInfo.weighted_bipred_idc);
    EXPECT_EQ(-8, ppsInfo.pic_init_qp_minus26);
    EXPECT_EQ(0, ppsInfo.pic_init_qs_minus26);
    EXPECT_EQ(-2, ppsInfo.chroma_qp_index_offset);
    EXPECT_EQ(1, ppsInfo.deblocking_filter_control_present_flag);
    EXPECT_EQ(0, ppsInfo.constrained_intra_pred_flag);
    EXPECT_EQ(0, ppsInfo.redundant_pic_cnt_present_flag);


}

TEST(TestH26xPPS, TestParsePPS2)
{
	
	const std::vector<BYTE> ppsData = {0xe8, 0x43, 0x82, 0x92, 0xc8, 0xb0};


	H264PictureParameterSet ppsParser;

	ppsParser.Decode(ppsData.data(), ppsData.size());
	// ppsParser.Dump();

    auto ppsInfo = ppsParser.GetPPSData();

    EXPECT_EQ(0, ppsInfo.pic_parameter_set_id);
    EXPECT_EQ(0, ppsInfo.seq_parameter_set_id);
    EXPECT_EQ(1, ppsInfo.entropy_coding_mode_flag);
    EXPECT_EQ(0, ppsInfo.pic_order_present_flag);
    EXPECT_EQ(0, ppsInfo.num_slice_groups_minus1);
    EXPECT_EQ(15, ppsInfo.num_ref_idx_l0_active_minus1);
    EXPECT_EQ(0, ppsInfo.num_ref_idx_l1_active_minus1);
    EXPECT_EQ(1, ppsInfo.weighted_pred_flag);
    EXPECT_EQ(2, ppsInfo.weighted_bipred_idc);
    EXPECT_EQ(10, ppsInfo.pic_init_qp_minus26);
    EXPECT_EQ(0, ppsInfo.pic_init_qs_minus26);
    EXPECT_EQ(-2, ppsInfo.chroma_qp_index_offset);
    EXPECT_EQ(1, ppsInfo.deblocking_filter_control_present_flag);
    EXPECT_EQ(0, ppsInfo.constrained_intra_pred_flag);
    EXPECT_EQ(0, ppsInfo.redundant_pic_cnt_present_flag);
    // We are not yet parsing these params? leaving it here as a comment for future reference
    // EXPECT_EQ(1, ppsInfo.transform_8x8_mode_flag);
    // EXPECT_EQ(0, ppsInfo.pic_scaling_matrix_present_flag);
    // EXPECT_EQ(-2, ppsInfo.second_chroma_qp_index_offset);

	

}