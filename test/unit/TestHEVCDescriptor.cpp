#include "TestCommon.h"

//HACK: allow accessing private attributes
//TODO: expose all in getters
#define private public
#include "h265/HEVCDescriptor.h"
#undef  private

TEST(TestHEVCDescriptor, Parse)
{
	Logger::EnableDebug(true);

	/*
	*
        configurationVersion  =  1
        general_profile_space  =  0
        general_tier_flag  =  0
        general_profile_idc  =  1
        general_profile_compatibility_flags  =  1073741824 (0x40000000)
        general_constraint_indicator_flags  =  158329674399744 (0x900000000000)
        general_level_idc  =  123
        min_spatial_segmentation_idc  =  0
        parallelismType  =  0
        chromaFormat  =  1
        bitDepthLumaMinus8  =  0
        bitDepthChromaMinus8  =  0
        avgFrameRate  =  0
        constantFrameRate  =  0
        numTemporalLayers  =  1
        temporalIdNested  =  1
        lengthSizeMinusOne  =  3
        numOfArrays  =  3
        for (i = 0; i < numOfArrays; i++)
          array_completeness[0]  =  0
          NAL_unit_type[0]  =  32
          numNalus[0]  =  1
          for (j=0; j< numNalus; j++)
            nalUnitLength[0][0]  =  24
            HEVC Video Parameter Set (VPS_NUT) [0][0]
              forbidden_zero_bit  =  0
              nal_unit_type  =  32 (VPS_NUT)
              nuh_layer_id  =  0
              nuh_temporal_id_plus1  =  1
              vps_video_parameter_set_id   =  0
              vps_base_layer_internal_flag   =  1
              vps_base_layer_available_flag   =  1
              vps_max_layers_minus1   =  0
              vps_max_sub_layers_minus1   =  0
              vps_temporal_id_nesting_flag   =  1
              profile_tier_level()
                general_profile_space  =  0
                general_tier_flag  =  0
                general_profile_idc  =  1
                for (i = 0; i < 32; i++)
                  general_profile_compatibility_flag[ 0: 7]  =   0,  1,  0,  0,  0,  0,  0,  0
                  general_profile_compatibility_flag[ 8:15]  =   0,  0,  0,  0,  0,  0,  0,  0
                  general_profile_compatibility_flag[16:23]  =   0,  0,  0,  0,  0,  0,  0,  0
                  general_profile_compatibility_flag[24:31]  =   0,  0,  0,  0,  0,  0,  0,  0
                general_progressive_source_flag  =  1
                general_interlaced_source_flag  =  0
                general_non_packed_constraint_flag  =  0
                general_frame_only_constraint_flag  =  1
                general_reserved_zero_43bits  =  0
                if((general_profile_idc >= 1 || general_profile_idc <= 5) || general_profile_idc == 9 || general_profile_compatibility_flag[1] || general_profile_compatibility_flag[2] || general_profile_compatibility_flag[3] || general_profile_compatibility_flag[4] || general_profile_compatibility_flag[5] || general_profile_compatibility_flag[9])
                  general_inbld_flag  =  0
                general_level_idc  =  123
              vps_sub_layer_ordering_info_present_flag   =  1
              for (i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1); i <= vps_max_sub_layers_minus1; i++)
                vps_max_dec_pic_buffering_minus1[0]  =  4
                vps_max_num_reorder_pics[0]  =  0
                vps_max_latency_increase[0]  =  0
              vps_max_layer_id  =  0
              vps_num_layer_sets_minus1  =  0
              vps_timing_info_present_flag  =  0
              vps_extension_flag  =  0
          array_completeness[1]  =  0
          NAL_unit_type[1]  =  33
          numNalus[1]  =  1
          for (j=0; j< numNalus; j++)
            nalUnitLength[1][0]  =  61
            HEVC Sequence Parameter Set (SPS_NUT) [1][0]
              forbidden_zero_bit  =  0
              nal_unit_type  =  33 (SPS_NUT)
              nuh_layer_id  =  0
              nuh_temporal_id_plus1  =  1
              sps_video_parameter_set_id  =  0
              sps_max_sub_layers_minus1  =  0
              sps_temporal_id_nesting_flag  =  1
              profile_tier_level()
                general_profile_space  =  0
                general_tier_flag  =  0
                general_profile_idc  =  1
                for (i = 0; i < 32; i++)
                  general_profile_compatibility_flag[ 0: 7]  =   0,  1,  0,  0,  0,  0,  0,  0
                  general_profile_compatibility_flag[ 8:15]  =   0,  0,  0,  0,  0,  0,  0,  0
                  general_profile_compatibility_flag[16:23]  =   0,  0,  0,  0,  0,  0,  0,  0
                  general_profile_compatibility_flag[24:31]  =   0,  0,  0,  0,  0,  0,  0,  0
                general_progressive_source_flag  =  1
                general_interlaced_source_flag  =  0
                general_non_packed_constraint_flag  =  0
                general_frame_only_constraint_flag  =  1
                general_reserved_zero_43bits  =  0
                if((general_profile_idc >= 1 || general_profile_idc <= 5) || general_profile_idc == 9 || general_profile_compatibility_flag[1] || general_profile_compatibility_flag[2] || general_profile_compatibility_flag[3] || general_profile_compatibility_flag[4] || general_profile_compatibility_flag[5] || general_profile_compatibility_flag[9])
                  general_inbld_flag  =  0
                general_level_idc  =  123
              sps_seq_parameter_set_id  =  0
              chroma_format_idc  =  1
              pic_width_in_luma_samples  =  1920
              pic_height_in_luma_samples  =  1088
              conformance_window_flag  =  1
              if (conformance_window_flag)
                conf_win_left_offset  =  0
                conf_win_right_offset  =  0
                conf_win_top_offset  =  0
                conf_win_bottom_offset  =  4
              bit_depth_luma_minus8  =  0
              bit_depth_chroma_minus8  =  0
              log2_max_pic_order_cnt_lsb_minus4  =  4
              sps_sub_layer_ordering_info_present_flag  =  1
              for (i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1); i <= sps_max_sub_layers_minus1; i++)
                sps_max_dec_pic_buffering_minus1[0]  =  4
                sps_max_num_reorder_pics[0]  =  0
                sps_max_latency_increase[0]  =  0
              log2_min_luma_coding_block_size_minus3  =  1
              log2_diff_max_min_luma_coding_block_size  =  1
              log2_min_transform_block_size_minus2  =  0
              log2_diff_max_min_transform_block_size  =  3
              max_transform_hierarchy_depth_inter  =  3
              max_transform_hierarchy_depth_intra  =  3
              scaling_list_enabled_flag  =  0
              amp_enabled_flag  =  1
              sample_adaptive_offset_enabled_flag  =  1
              pcm_enabled_flag  =  0
              num_short_term_ref_pic_sets  =  1
              for (i = 0; i < num_short_term_ref_pic_sets; i++)
                short_term_ref_pic_set(0)
                  if (!inter_ref_pic_set_prediction_flag)
                    num_negative_pics  =  4
                    num_positive_pics  =  0
                    for (i = 0; i < num_negative_pics; i++)
                      delta_poc_s0_minus1[0]  =  0
                      used_by_curr_pic_s0_flag[0]  =  1
                      delta_poc_s0_minus1[1]  =  0
                      used_by_curr_pic_s0_flag[1]  =  1
                      delta_poc_s0_minus1[2]  =  0
                      used_by_curr_pic_s0_flag[2]  =  1
                      delta_poc_s0_minus1[3]  =  0
                      used_by_curr_pic_s0_flag[3]  =  1
              long_term_ref_pics_present_flag  =  0
              sps_temporal_mvp_enabled_flag  =  0
              strong_intra_smoothing_enabled_flag  =  0
              vui_parameters_present_flag  =  1
              if (vui_parameters_present_flag)
                vui_parameters()
                  aspect_ratio_info_present_flag  =  1
                  if (aspect_ratio_info_present_flag)
                    aspect_ratio_idc  =  1 ( 1:1 square )
                  overscan_info_present_flag  =  0
                  video_signal_type_present_flag  =  1
                  if (video_signal_type_present_flag)
                    video_format  =  5
                    video_full_range_flag  =  0
                    colour_description_present_flag  =  1
                    if (colour_description_present_flag)
                      colour_primaries  =  1
                      transfer_characteristics  =  1
                      matrix_coeffs  =  1
                  chroma_loc_info_present_flag  =  0
                  neutral_chroma_indication_flag  =  0
                  field_seq_flag  =  0
                  frame_field_info_present_flag  =  0
                  default_display_window_flag  =  0
                  vui_timing_info_present_flag  =  1
                  if (vui_timing_info_present_flag)
                    vui_num_units_in_tick  =  1
                    vui_time_scale  =  60
                    vui_poc_proportional_to_timing_flag  =  0
                      vui_hrd_parameters_present_flag  =  1
                      if (vui_hrd_parameters_present_flag)
                        hrd_parameters(1,  sps_max_sub_layers_minus1)
                          if (1)
                            nal_hrd_parameters_present_flag  =  1
                            vcl_hrd_parameters_present_flag  =  0
                            if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
                              sub_pic_cpb_params_present_flag  =  0
                              bit_rate_scale  =  0
                              cpb_size_scale  =  0
                              initial_cpb_removal_delay_length_minus1  =  23
                              au_cpb_removal_delay_length_minus1  =  15
                              dpb_output_delay_length_minus1  =  5
                          for (i = 0; i <=  sps_max_sub_layers_minus1; i++)
                            fixed_pic_rate_general_flag[0]  =  0
                            if (fixed_pic_rate_general_flag[i])
                              fixed_pic_rate_within_cvs_flag[0]  =  0
                            if (!fixed_pic_rate_within_cvs_flag[i])
                              low_delay_hrd_flag[0]  =  0
                            if (!low_delay_hrd_flag[i])
                              cpb_cnt_minus1[0]  =  0
                            if (nal_hrd_parameters_present_flag)
                              sub_layer_hrd_parameters()
                                for (i = 0; i <= CpbCnt; i++)
                                  bit_rate_value_minus1[0]  =  156249
                                  cpb_size_value_minus1[0]  =  156249
                                  cbr_flag[0]  =  0
                  bitstream_restriction_flag  =  0
              sps_extension_flag  =  0
          array_completeness[2]  =  0
          NAL_unit_type[2]  =  34
          numNalus[2]  =  1
          for (j=0; j< numNalus; j++)
            nalUnitLength[2][0]  =  7
            HEVC Picture Parameter Set (PPS_NUT) [2][0]
              forbidden_zero_bit  =  0
              nal_unit_type  =  34 (PPS_NUT)
              nuh_layer_id  =  0
              nuh_temporal_id_plus1  =  1
              pps_pic_parameter_set_id  =  0
              pps_seq_parameter_set_id  =  0
              dependent_slice_segments_enabled_flag  =  0
              output_flag_present_flag  =  0
              num_extra_slice_header_bits  =  0
              sign_data_hiding_flag  =  0
              cabac_init_present_flag  =  1
              num_ref_idx_l0_default_active_minus1  =  3
              num_ref_idx_l1_default_active_minus1  =  0
              init_qp_minus26  =  0
              constrained_intra_pred_flag  =  0
              transform_skip_enabled_flag  =  1
              cu_qp_delta_enabled_flag  =  1
              if (cu_qp_delta_enabled_flag)
                diff_cu_qp_delta_depth  =  0
              pps_cb_qp_offset  =  0
              pps_cr_qp_offset  =  0
              pps_slice_chroma_qp_offsets_present_flag  =  0
              weighted_pred_flag  =  0
              weighted_bipred_flag  =  0
              transquant_bypass_enabled_flag  =  0
              tiles_enabled_flag  =  0
              entropy_coding_sync_enabled_flag  =  0
              loop_filter_across_slices_enabled_flag  =  1
              deblocking_filter_control_present_flag  =  1
              if (deblocking_filter_control_present_flag)
                deblocking_filter_override_enabled_flag  =  0
                pps_deblocking_filter_disabled_flag  =  0
                if (!pps_deblocking_filter_disabled_flag)
                  pps_beta_offset_div2  =  0
                  pps_tc_offset_div2  =  0
              pps_scaling_list_data_present_flag  =  0
              lists_modification_present_flag  =  0
              log2_parallel_merge_level_minus2  =  0
              slice_segment_header_extension_present_flag  =  0
              pps_extension_flag  =  0
	*/
        const uint8_t data[] = {
                0x01, 0x01, 0x40, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7b, 0xf0, 0x00, 0xfc,
                0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x0f, 0x03, 0x20, 0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0c, 0x01,
                0xff, 0xff, 0x01, 0x40, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
                0x7b, 0x97, 0x02, 0x40, 0x21, 0x00, 0x01, 0x00, 0x3d, 0x42, 0x01, 0x01, 0x01, 0x40, 0x00, 0x00,
                0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x7b, 0xa0, 0x03, 0xc0, 0x80, 0x11,
                0x07, 0xcb, 0x96, 0x5d, 0x29, 0x08, 0x46, 0x45, 0xff, 0x8c, 0x05, 0xa8, 0x08, 0x08, 0x08, 0x20,
                0x00, 0x00, 0x03, 0x00, 0x20, 0x00, 0x00, 0x07, 0x8c, 0x00, 0xbb, 0xca, 0x20, 0x00, 0x09, 0x89,
                0x68, 0x00, 0x01, 0x31, 0x2d, 0x08, 0x22, 0x00, 0x01, 0x00, 0x07, 0x44, 0x01, 0xc0, 0x93, 0x7c,
                0x0c, 0xc9, 0x00, 0x00, 0x00, 0x13
        };


        HEVCDescriptor descriptor;

        ASSERT_TRUE(descriptor.Parse(data,sizeof(data)));

        EXPECT_EQ(descriptor.configurationVersion       , 1);
        EXPECT_EQ(descriptor.generalProfileSpace        , 0);
        EXPECT_EQ(descriptor.generalTierFlag            , 0);
        EXPECT_EQ(descriptor.generalProfileIdc          , 1);
        EXPECT_EQ(descriptor.profileIndication          , 1);
        EXPECT_EQ(descriptor.generalProfileCompatibilityFlags   , 0x40000000);
        //EXPECT_EQ(descriptor.generalConstraintIndicatorFlags    , { 0x90, 0x00, 0x00, 0x00, 0x00, 0x00 });
        EXPECT_EQ(descriptor.generalLevelIdc            , 123);
        //EXPECT_EQ(descriptor.paddingBytes               , { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );
        EXPECT_EQ(descriptor.NALUnitLengthSizeMinus1    , 3);
        EXPECT_EQ(descriptor.numOfArrays                , 3);

        EXPECT_EQ(descriptor.numOfVideoParameterSets    , 1);
        EXPECT_EQ(descriptor.vpsData.size()             , descriptor.numOfVideoParameterSets);
        EXPECT_EQ(descriptor.vpsData[0].GetSize()       , 24);
        EXPECT_EQ(descriptor.vpsData[0].GetSize()       , descriptor.vpsTotalSizes);

        EXPECT_EQ(descriptor.numOfSequenceParameterSets , 1);
        EXPECT_EQ(descriptor.spsData.size()             , descriptor.numOfSequenceParameterSets);
        EXPECT_EQ(descriptor.spsData[0].GetSize()       , 61);
        EXPECT_EQ(descriptor.spsData[0].GetSize()       , descriptor.spsTotalSizes);

        EXPECT_EQ(descriptor.numOfPictureParameterSets  , 1);
        EXPECT_EQ(descriptor.ppsData.size()             , descriptor.numOfVideoParameterSets);
        EXPECT_EQ(descriptor.ppsData[0].GetSize()       , 7);
        EXPECT_EQ(descriptor.ppsData[0].GetSize()       , descriptor.ppsTotalSizes);


        //descriptor.Dump();
}