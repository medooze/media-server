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
	bool Decode(BYTE* buffer,DWORD bufferSize)
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
		//Free memory
		free(aux);
		//OK
		return !r.Error();
	}
private:
	inline static DWORD Escape( BYTE *dst, BYTE *src, DWORD size )
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
	DWORD GetWidth()	{ return (pic_width_in_mbs_minus1+1)*16; }
	DWORD GetHeight()	{ return (pic_height_in_map_units_minus1+1)*16; }
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
		Debug("/]\n");
	}
private:
	BYTE			profile_idc = 0;
	BYTE			constraint_set0_flag = 0;
	BYTE			constraint_set1_flag = 0;
	BYTE			constraint_set2_flag = 0;
	BYTE			reserved_zero_5bits  = 0;
	BYTE			level_idc = 0;
	BYTE			seq_parameter_set_id = 0;
	BYTE			log2_max_frame_num_minus4 = 0;
	BYTE			pic_order_cnt_type = 0;
	BYTE			log2_max_pic_order_cnt_lsb_minus4 = 0;
	BYTE			delta_pic_order_always_zero_flag = 0;
	int			offset_for_non_ref_pic = 0;
	int			offset_for_top_to_bottom_field = 0;
	BYTE			num_ref_frames_in_pic_order_cnt_cycle = 0;
	std::vector<int>	offset_for_ref_frame;
	BYTE			num_ref_frames = 0;
	BYTE			gaps_in_frame_num_value_allowed_flag = 0;
	BYTE			pic_width_in_mbs_minus1 = 0;
	BYTE			pic_height_in_map_units_minus1 = 0;
	BYTE			frame_mbs_only_flag = 0;
};

class H264PictureParameterSet
{
public:
	bool Decode(BYTE* buffer,DWORD bufferSize)
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
		CHECK(r); pic_init_qp_minus26 = ExpGolombDecoder::Decode(r); //Signed
		CHECK(r); pic_init_qs_minus26 = ExpGolombDecoder::Decode(r); //Signed
		CHECK(r); chroma_qp_index_offset = ExpGolombDecoder::Decode(r); //Signed
		CHECK(r); deblocking_filter_control_present_flag = r.Get(1);
		CHECK(r); constrained_intra_pred_flag = r.Get(1);
		CHECK(r); redundant_pic_cnt_present_flag = r.Get(1);

		//Free memory
		free(aux);
		//OK
		return true;
	}
private:
	inline static DWORD Escape( BYTE *dst, BYTE *src, DWORD size )
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
	BYTE			entropy_coding_mode_flag = 0;
	BYTE			pic_order_present_flag = 0;
	int			num_slice_groups_minus1 = 0;
	BYTE			slice_group_map_type = 0;
	std::vector<DWORD>	run_length_minus1;
	std::vector<DWORD>	top_left;
	std::vector<DWORD>	bottom_right;
	BYTE			slice_group_change_direction_flag = 0;
	int			slice_group_change_rate_minus1 = 0;
	int			pic_size_in_map_units_minus1 = 0;
	std::vector<DWORD>	slice_group_id;
	BYTE			num_ref_idx_l0_active_minus1 = 0;
	BYTE			num_ref_idx_l1_active_minus1 = 0;
	BYTE			weighted_pred_flag = 0;
	BYTE			weighted_bipred_idc = 0;
	int			pic_init_qp_minus26 = 0;
	int			pic_init_qs_minus26 = 0;
	int			chroma_qp_index_offset = 0;
	BYTE			deblocking_filter_control_present_flag = 0;
	BYTE			constrained_intra_pred_flag = 0;
	BYTE			redundant_pic_cnt_present_flag = 0;
};

#endif	/* H264_H */

