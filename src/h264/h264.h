/* 
 * File:   h264.h
 * Author: Sergio
 *
 * Created on 18 de julio de 2012, 17:12
 */

#ifndef H264_H
#define	H264_H
#include "config.h"
#include "math.h"
#include "bitstream.h"

#define CHECK(r) if(r.Error()) return false;

class H264SeqParameterSet
{
public:
	bool Decode(const BYTE* buffer,DWORD bufferSize)
	{
		//SHould be done otherway, like modifying the BitReader to escape the input NAL, but anyway.. duplicate memory
		BYTE *aux = (BYTE*)malloc(bufferSize);
		//Escape
		DWORD len = Escape(aux,buffer,bufferSize);
		//Create bit reader
		BitReader r(aux,len);
		//Read SPS
		CHECK(r); profile_idc = r.Get(8);
		CHECK(r); constraint_set0_flag = r.Get(1);
		CHECK(r); constraint_set1_flag = r.Get(1);
		CHECK(r); constraint_set2_flag = r.Get(1);
		CHECK(r); reserved_zero_5bits  = r.Get(5);
		CHECK(r); level_idc = r.Get(8);
		CHECK(r); seq_parameter_set_id = ExpGolombDecoder::Decode(r);
		
		//Check profile
		if(profile_idc==100 || profile_idc==110 || profile_idc==122 || profile_idc==244 || profile_idc==44 || 
			profile_idc==83 || profile_idc==86 || profile_idc==118 || profile_idc==128 || profile_idc==138 || profile_idc==139 || 
			profile_idc==134 || profile_idc==135)
		{
			CHECK(r); auto chroma_format_idc = ExpGolombDecoder::Decode(r);
			if( chroma_format_idc == 3 )
			{
				CHECK(r); auto separate_colour_plane_flag = r.Get(1);
			}
			CHECK(r); auto bit_depth_luma_minus8 = ExpGolombDecoder::Decode(r);
			CHECK(r); auto bit_depth_chroma_minus8 = ExpGolombDecoder::Decode(r);
			CHECK(r); auto qpprime_y_zero_transform_bypass_flag = r.Get(1);
			CHECK(r); auto seq_scaling_matrix_present_flag = r.Get(1);
			if( seq_scaling_matrix_present_flag )
			{	
				for(uint32_t i = 0; i < (( chroma_format_idc != 3 ) ? 8 : 12 ); i++ )
				{
					CHECK(r); auto seq_scaling_list_present_flag = r.Get(1);
					if( seq_scaling_list_present_flag)
					{
						//Not supported
						return false;
//						if( i < 6 )
//							scaling_list( ScalingList4x4[ i ], 16, UseDefaultScalingMatrix4x4Flag[ i ] )
//						else
//							scaling_list( ScalingList8x8[ i − 6 ], 64, UseDefaultScalingMatrix8x8Flag[ i − 6 ] )
					}
				}
			}
		}
		CHECK(r); log2_max_frame_num_minus4 = ExpGolombDecoder::Decode(r);
		pic_order_cnt_type = ExpGolombDecoder::Decode(r);
		if( pic_order_cnt_type == 0 )
		{
			CHECK(r); log2_max_pic_order_cnt_lsb_minus4 = ExpGolombDecoder::Decode(r);
		} else if( pic_order_cnt_type == 1 ) {
			CHECK(r); delta_pic_order_always_zero_flag = r.Get(1);
			CHECK(r); offset_for_non_ref_pic = ExpGolombDecoder::Decode(r); 
			CHECK(r); offset_for_top_to_bottom_field = ExpGolombDecoder::Decode(r); 
			CHECK(r); num_ref_frames_in_pic_order_cnt_cycle = ExpGolombDecoder::Decode(r);
			for( DWORD i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
			{
				CHECK(r); offset_for_ref_frame.assign(i,ExpGolombDecoder::Decode(r));
			}
		}
		CHECK(r); num_ref_frames = ExpGolombDecoder::Decode(r);
		CHECK(r); gaps_in_frame_num_value_allowed_flag = r.Get(1);
		CHECK(r); pic_width_in_mbs_minus1 = ExpGolombDecoder::Decode(r);
		CHECK(r); pic_height_in_map_units_minus1 = ExpGolombDecoder::Decode(r);
		CHECK(r); frame_mbs_only_flag = r.Get(1);
		if (!frame_mbs_only_flag)
		{
			CHECK(r); mb_adaptive_frame_field_flag = r.Get(1);
		}
		CHECK(r); direct_8x8_inference_flag = r.Get(1);
		CHECK(r); frame_cropping_flag = r.Get(1);
		if (frame_cropping_flag)
		{
			CHECK(r); frame_crop_left_offset = ExpGolombDecoder::Decode(r);
			CHECK(r); frame_crop_right_offset = ExpGolombDecoder::Decode(r);
			CHECK(r); frame_crop_top_offset = ExpGolombDecoder::Decode(r);
			CHECK(r); frame_crop_bottom_offset = ExpGolombDecoder::Decode(r);
		}
		//CHECK(r); vui_parameters_present_flag = r.Get(1);
		//Free memory
		free(aux);
		//OK
		return !r.Error();
	}
private:
	inline static DWORD Escape( BYTE *dst,const BYTE *src, DWORD size )
	{
		DWORD len = 0;
		DWORD i = 0;
		while(i<size)
		{
			//Check if next BYTEs are the scape sequence
	                if((i+2<size) && (get3(src,i)==0x03))
			{
				//Copy the first two
	                        dst[len++] = get1(src,i);
	                        dst[len++] = get1(src,i+1);
				//Skip the three
	                        i += 3;
	                } else {
	                        dst[len++] = get1(src,i++);
	                }
	        }
		return len;
	}
public:
	DWORD GetWidth()	{ return ((pic_width_in_mbs_minus1 +1)*16) - frame_crop_right_offset *2 - frame_crop_left_offset *2; }
	DWORD GetHeight()	{ return ((2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 +1) * 16) - frame_crop_bottom_offset*2 - frame_crop_top_offset*2; }
	void Dump() const
	{
		Debug("[H264SeqParameterSet \n");
		Debug("\tprofile_idc=%.2x\n",				profile_idc);
		Debug("\tconstraint_set0_flag=%u\n",			constraint_set0_flag);
		Debug("\tconstraint_set1_flag=%u\n",			constraint_set1_flag);
		Debug("\tconstraint_set2_flag=%u\n",			constraint_set2_flag);
		Debug("\treserved_zero_5bits =%u\n",			reserved_zero_5bits );
		Debug("\tlevel_idc=%.2x\n",				level_idc);
		Debug("\tseq_parameter_set_id=%u\n",			seq_parameter_set_id);
		Debug("\tlog2_max_frame_num_minus4=%u\n",		log2_max_frame_num_minus4);
		Debug("\tpic_order_cnt_type=%u\n",			pic_order_cnt_type);
		Debug("\tlog2_max_pic_order_cnt_lsb_minus4=%u\n",	log2_max_pic_order_cnt_lsb_minus4);
		Debug("\t delta_pic_order_always_zero_flag=%u\n",	delta_pic_order_always_zero_flag);
		Debug("\toffset_for_non_ref_pic=%d\n",			offset_for_non_ref_pic);
		Debug("\toffset_for_top_to_bottom_field=%d\n",		offset_for_top_to_bottom_field);
		Debug("\tnum_ref_frames_in_pic_order_cnt_cycle=%u\n",	num_ref_frames_in_pic_order_cnt_cycle);
		Debug("\tnum_ref_frames=%u\n",				num_ref_frames);
		Debug("\tgaps_in_frame_num_value_allowed_flag=%u\n",	gaps_in_frame_num_value_allowed_flag);
		Debug("\tpic_width_in_mbs_minus1=%u\n",			pic_width_in_mbs_minus1);
		Debug("\tpic_height_in_map_units_minus1=%u\n",		pic_height_in_map_units_minus1);
		Debug("\tframe_mbs_only_flag=%u\n",			frame_mbs_only_flag);
		Debug("\tmb_adaptive_frame_field_flag=%u\n",		mb_adaptive_frame_field_flag);
		Debug("\tdirect_8x8_inference_flag=%u\n",		direct_8x8_inference_flag);
		Debug("\tframe_cropping_flag=%u\n",			frame_cropping_flag);
		Debug("\tframe_crop_left_offset=%u\n",			frame_crop_left_offset);
		Debug("\tframe_crop_right_offset=%u\n",			frame_crop_right_offset);
		Debug("\tframe_crop_top_offset=%u\n",			frame_crop_top_offset);
		Debug("\tframe_crop_bottom_offset=%u\n",		frame_crop_bottom_offset);
		Debug("/]\n");
	}
private:
	BYTE			profile_idc = 0;
	bool			constraint_set0_flag = false;
	bool			constraint_set1_flag = false;
	bool			constraint_set2_flag = false;
	BYTE			reserved_zero_5bits  = 0;
	BYTE			level_idc = 0;
	BYTE			seq_parameter_set_id = 0;
	BYTE			log2_max_frame_num_minus4 = 0;
	BYTE			pic_order_cnt_type = 0;
	BYTE			log2_max_pic_order_cnt_lsb_minus4 = 0;
	bool			delta_pic_order_always_zero_flag = false;
	int			offset_for_non_ref_pic = 0;
	int			offset_for_top_to_bottom_field = 0;
	BYTE			num_ref_frames_in_pic_order_cnt_cycle = 0;
	std::vector<int>	offset_for_ref_frame;
	DWORD			num_ref_frames = 0;
	bool			gaps_in_frame_num_value_allowed_flag = false;
	DWORD			pic_width_in_mbs_minus1 = 0;
	DWORD			pic_height_in_map_units_minus1 = 0;
	bool			frame_mbs_only_flag = false;
	bool			mb_adaptive_frame_field_flag = false;
	bool			direct_8x8_inference_flag = false;
	bool			frame_cropping_flag = false;
	DWORD			frame_crop_left_offset = 0;
	DWORD			frame_crop_right_offset = 0;
	DWORD			frame_crop_top_offset = 0;
	DWORD			frame_crop_bottom_offset = 0;
	//bool			vui_parameters_present_flag = false;

};

class H264PictureParameterSet
{
public:
	bool Decode(const BYTE* buffer,DWORD bufferSize)
	{
		//SHould be done otherway, like modifying the BitReader to escape the input NAL, but anyway.. duplicate memory
		BYTE *aux = (BYTE*)malloc(bufferSize);
		//Escape
		DWORD len = Escape(aux,buffer,bufferSize);
		//Create bit reader
		BitReader r(aux,len);
		//Read SQS
		CHECK(r); pic_parameter_set_id = ExpGolombDecoder::Decode(r);
		CHECK(r); seq_parameter_set_id = ExpGolombDecoder::Decode(r);
		CHECK(r); entropy_coding_mode_flag = r.Get(1);
		CHECK(r); pic_order_present_flag = r.Get(1);
		CHECK(r); num_slice_groups_minus1 = ExpGolombDecoder::Decode(r);
		if( num_slice_groups_minus1 > 0 )
		{
			CHECK(r); slice_group_map_type = ExpGolombDecoder::Decode(r);
			if( slice_group_map_type == 0 )
				for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )
				{
					CHECK(r); run_length_minus1.assign(iGroup,ExpGolombDecoder::Decode(r));
				}
			else if( slice_group_map_type == 2 )
				for( int iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ )
				{
					CHECK(r); top_left.assign(iGroup,ExpGolombDecoder::Decode(r));
					CHECK(r); bottom_right.assign(iGroup,ExpGolombDecoder::Decode(r));
				}
			else if( slice_group_map_type == 3 || slice_group_map_type ==  4 || slice_group_map_type == 5 )
			{
				CHECK(r); slice_group_change_direction_flag = r.Get(1);
				CHECK(r); slice_group_change_rate_minus1 = ExpGolombDecoder::Decode(r);
			} else if( slice_group_map_type == 6 ) {
				CHECK(r); pic_size_in_map_units_minus1 = ExpGolombDecoder::Decode(r);
				for( int i = 0; i <= pic_size_in_map_units_minus1; i++ )
				{
					CHECK(r); slice_group_id.assign(i,r.Get(ceil(log2(num_slice_groups_minus1+1))));
				}
			}
		}
		CHECK(r); num_ref_idx_l0_active_minus1 = ExpGolombDecoder::Decode(r);
		CHECK(r); num_ref_idx_l1_active_minus1 = ExpGolombDecoder::Decode(r);
		CHECK(r); weighted_pred_flag = r.Get(1);
		CHECK(r); weighted_bipred_idc = r.Get(2);
		CHECK(r); pic_init_qp_minus26 = ExpGolombDecoder::DecodeSE(r); //Signed
		CHECK(r); pic_init_qs_minus26 = ExpGolombDecoder::DecodeSE(r); //Signed
		CHECK(r); chroma_qp_index_offset = ExpGolombDecoder::DecodeSE(r); //Signed
		CHECK(r); deblocking_filter_control_present_flag = r.Get(1);
		CHECK(r); constrained_intra_pred_flag = r.Get(1);
		CHECK(r); redundant_pic_cnt_present_flag = r.Get(1);

		//Free memory
		free(aux);
		//OK
		return true;
	}
private:
	inline static DWORD Escape( BYTE *dst,const BYTE *src, DWORD size )
	{
		DWORD len = 0;
		DWORD i = 0;
		while(i<size)
		{
			//Check if next BYTEs are the scape sequence
	                if((i+2<size) && (get3(src,i)==0x03))
			{
				//Copy the first two
	                        dst[len++] = get1(src,i);
	                        dst[len++] = get1(src,i+1);
				//Skip the three
	                        i += 3;
	                } else {
	                        dst[len++] = get1(src,i++);
	                }
	        }
		return len;
	}
public:
	void Dump() const
	{
		Debug("[H264PictureParameterSet \n");
		Debug("\tpic_parameter_set_id=%u\n",			pic_parameter_set_id);
		Debug("\tseq_parameter_set_id=%u\n",			seq_parameter_set_id);
		Debug("\tentropy_coding_mode_flag=%u\n",		entropy_coding_mode_flag);
		Debug("\tpic_order_present_flag=%u\n",			pic_order_present_flag);
		Debug("\tnum_slice_groups_minus1=%d\n",			num_slice_groups_minus1);
		Debug("\tslice_group_map_type=%u\n",			slice_group_map_type);
		Debug("\tslice_group_change_direction_flag=%u\n",	slice_group_change_direction_flag);
		Debug("\tslice_group_change_rate_minus1=%u\n",		slice_group_change_rate_minus1);
		Debug("\tpic_size_in_map_units_minus1=%d\n",		pic_size_in_map_units_minus1);
		Debug("\tnum_ref_idx_l0_active_minus1=%d\n",		num_ref_idx_l0_active_minus1);
		Debug("\tnum_ref_idx_l1_active_minus1=%d\n",		num_ref_idx_l1_active_minus1);
		Debug("\tweighted_pred_flag=%u\n",			weighted_pred_flag);
		Debug("\tweighted_bipred_idc=%u\n",			weighted_bipred_idc);
		Debug("\tpic_init_qp_minus26=%d\n",			pic_init_qp_minus26);
		Debug("\tpic_init_qs_minus26=%d\n",			pic_init_qs_minus26);
		Debug("\tchroma_qp_index_offset=%d\n",			chroma_qp_index_offset);
		Debug("\tdeblocking_filter_control_present_flag=%u\n",	deblocking_filter_control_present_flag);
		Debug("\tconstrained_intra_pred_flag=%u\n",		constrained_intra_pred_flag);
		Debug("\tredundant_pic_cnt_present_flag=%u\n",		redundant_pic_cnt_present_flag);
		Debug("/]\n");
	}
private:
	BYTE			pic_parameter_set_id = 0;
	BYTE			seq_parameter_set_id = 0;
	bool			entropy_coding_mode_flag = false;
	bool			pic_order_present_flag = false;
	int			num_slice_groups_minus1 = 0;
	BYTE			slice_group_map_type = 0;
	std::vector<DWORD>	run_length_minus1;
	std::vector<DWORD>	top_left;
	std::vector<DWORD>	bottom_right;
	bool			slice_group_change_direction_flag = false;
	int			slice_group_change_rate_minus1 = 0;
	int			pic_size_in_map_units_minus1 = 0;
	std::vector<DWORD>	slice_group_id;
	BYTE			num_ref_idx_l0_active_minus1 = 0;
	BYTE			num_ref_idx_l1_active_minus1 = 0;
	bool			weighted_pred_flag = false;
	BYTE			weighted_bipred_idc = 0;
	int			pic_init_qp_minus26 = 0;
	int			pic_init_qs_minus26 = 0;
	int			chroma_qp_index_offset = 0;
	bool			deblocking_filter_control_present_flag = false;
	bool			constrained_intra_pred_flag = false;
	bool			redundant_pic_cnt_present_flag = false;
};

#undef CHECK

#endif	/* H264_H */

