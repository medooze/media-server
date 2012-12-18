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
		profile_idc = r.Get(8);
		constraint_set0_flag = r.Get(1);
		constraint_set1_flag = r.Get(1);
		constraint_set2_flag = r.Get(1);
		reserved_zero_5bits  = r.Get(5);
		level_idc = r.Get(8);
		seq_parameter_set_id = ExpGolombDecoder::Decode(r);
		log2_max_frame_num_minus4 = ExpGolombDecoder::Decode(r);
		pic_order_cnt_type = ExpGolombDecoder::Decode(r);
		if( pic_order_cnt_type == 0 )
			log2_max_pic_order_cnt_lsb_minus4 = ExpGolombDecoder::Decode(r);
		else if( pic_order_cnt_type == 1 ) {
			delta_pic_order_always_zero_flag = r.Get(1);
			offset_for_non_ref_pic = ExpGolombDecoder::Decode(r); 
			offset_for_top_to_bottom_field = ExpGolombDecoder::Decode(r); 
			num_ref_frames_in_pic_order_cnt_cycle = ExpGolombDecoder::Decode(r);
			for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
				offset_for_ref_frame.assign(i,ExpGolombDecoder::Decode(r));
		}
		num_ref_frames = ExpGolombDecoder::Decode(r);
		gaps_in_frame_num_value_allowed_flag = r.Get(1);
		pic_width_in_mbs_minus1 = ExpGolombDecoder::Decode(r);
		pic_height_in_map_units_minus1 = ExpGolombDecoder::Decode(r);
		frame_mbs_only_flag = r.Get(1);
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
	DWORD GetWidth()	{ return (pic_width_in_mbs_minus1+1)*16; }
	DWORD GetHeight()	{ return (pic_height_in_map_units_minus1+1)*16; }
	void Dump()
	{
		Debug("[H264SeqParameterSet \n");
		Debug("\tprofile_idc=%.2x\n",				profile_idc);
		Debug("\tconstraint_set0_flag=%u\n",			constraint_set0_flag);
		Debug("\tconstraint_set1_flag=%u\n",			constraint_set1_flag);
		Debug("\tconstraint_set2_flag=%u\n",			constraint_set2_flag);
		Debug("\treserved_zero_5bits =%u\n",			reserved_zero_5bits );
		Debug("\tlevel_idc=%.2x\n",				level_idc);
		Debug("\tseq_parameter_set_id=%u\n",			seq_parameter_set_id);
		Debug("\tlog2_max_frame_num_minus4=%u\n",			log2_max_frame_num_minus4);
		Debug("\tpic_order_cnt_type=%u\n",			pic_order_cnt_type);
		Debug("\tlog2_max_pic_order_cnt_lsb_minus4=%u\n",		log2_max_pic_order_cnt_lsb_minus4);
		Debug("\t delta_pic_order_always_zero_flag=%u\n",		delta_pic_order_always_zero_flag);
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
	BYTE			profile_idc;
	BYTE			constraint_set0_flag;
	BYTE			constraint_set1_flag;
	BYTE			constraint_set2_flag;
	BYTE			reserved_zero_5bits ;
	BYTE			level_idc;
	DWORD			seq_parameter_set_id;
	DWORD			log2_max_frame_num_minus4;
	DWORD			pic_order_cnt_type;
	DWORD			log2_max_pic_order_cnt_lsb_minus4;
	BYTE			delta_pic_order_always_zero_flag;
	int			offset_for_non_ref_pic;
	int			offset_for_top_to_bottom_field;
	DWORD			num_ref_frames_in_pic_order_cnt_cycle;
	std::vector<int>	offset_for_ref_frame;
	DWORD			num_ref_frames;
	BYTE			gaps_in_frame_num_value_allowed_flag;
	BYTE			pic_width_in_mbs_minus1;
	BYTE			pic_height_in_map_units_minus1;
	BYTE			frame_mbs_only_flag;
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
		pic_parameter_set_id = ExpGolombDecoder::Decode(r);
		seq_parameter_set_id = ExpGolombDecoder::Decode(r);
		entropy_coding_mode_flag = r.Get(1);
		pic_order_present_flag = r.Get(1);
		num_slice_groups_minus1 = ExpGolombDecoder::Decode(r);
		if( num_slice_groups_minus1 > 0 )
		{
			slice_group_map_type = ExpGolombDecoder::Decode(r);
			if( slice_group_map_type == 0 )
				for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )
					run_length_minus1.assign(iGroup,ExpGolombDecoder::Decode(r));
			else if( slice_group_map_type == 2 )
				for( int iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ )
				{
					top_left.assign(iGroup,ExpGolombDecoder::Decode(r));
					bottom_right.assign(iGroup,ExpGolombDecoder::Decode(r));
				}
			else if( slice_group_map_type == 3 || slice_group_map_type ==  4 || slice_group_map_type == 5 )
			{
				slice_group_change_direction_flag = r.Get(1);
				slice_group_change_rate_minus1 = ExpGolombDecoder::Decode(r);
			} else if( slice_group_map_type == 6 ) {
				pic_size_in_map_units_minus1 = ExpGolombDecoder::Decode(r);
				for( int i = 0; i <= pic_size_in_map_units_minus1; i++ )
					slice_group_id.assign(i,r.Get(ceil(log2(num_slice_groups_minus1+1))));
			}
		}
		num_ref_idx_l0_active_minus1 = ExpGolombDecoder::Decode(r);
		num_ref_idx_l1_active_minus1 = ExpGolombDecoder::Decode(r);
		weighted_pred_flag = r.Get(1);
		weighted_bipred_idc = r.Get(2);
		pic_init_qp_minus26 = ExpGolombDecoder::Decode(r); //Signed
		pic_init_qs_minus26 = ExpGolombDecoder::Decode(r); //Signed
		chroma_qp_index_offset = ExpGolombDecoder::Decode(r); //Signed
		deblocking_filter_control_present_flag = r.Get(1);
		constrained_intra_pred_flag = r.Get(1);
		redundant_pic_cnt_present_flag = r.Get(1);

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
	void Dump()
	{
		Debug("[H264PictureParameterSet \n");
		Debug("\tpic_parameter_set_id=%u\n",			pic_parameter_set_id);
		Debug("\tseq_parameter_set_id=%u\n",			seq_parameter_set_id);
		Debug("\tentropy_coding_mode_flag=%u\n",			entropy_coding_mode_flag);
		Debug("\tpic_order_present_flag=%u\n",			pic_order_present_flag);
		Debug("\tnum_slice_groups_minus1=%u\n",			num_slice_groups_minus1);
		Debug("\tslice_group_map_type=%u\n",			slice_group_map_type);
		Debug("\tslice_group_change_direction_flag=%u\n",		slice_group_change_direction_flag);
		Debug("\tslice_group_change_rate_minus1=%u\n",		slice_group_change_rate_minus1);
		Debug("\tpic_size_in_map_units_minus1=%u\n",		pic_size_in_map_units_minus1);
		Debug("\tnum_ref_idx_l0_active_minus1=%u\n",		num_ref_idx_l0_active_minus1);
		Debug("\tnum_ref_idx_l1_active_minus1=%u\n",		num_ref_idx_l1_active_minus1);
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
	DWORD			pic_parameter_set_id;
	DWORD			seq_parameter_set_id;
	BYTE			entropy_coding_mode_flag;
	BYTE			pic_order_present_flag;
	DWORD			num_slice_groups_minus1;
	DWORD			slice_group_map_type;
	std::vector<DWORD>	run_length_minus1;
	std::vector<DWORD>	top_left;
	std::vector<DWORD>	bottom_right;
	BYTE			slice_group_change_direction_flag;
	DWORD			slice_group_change_rate_minus1;
	DWORD			pic_size_in_map_units_minus1;
	std::vector<DWORD>	slice_group_id;
	DWORD			num_ref_idx_l0_active_minus1;
	DWORD			num_ref_idx_l1_active_minus1;
	BYTE			weighted_pred_flag;
	BYTE			weighted_bipred_idc;
	int			pic_init_qp_minus26;
	int			pic_init_qs_minus26;
	int			chroma_qp_index_offset;
	BYTE			deblocking_filter_control_present_flag;
	BYTE			constrained_intra_pred_flag;
	BYTE			redundant_pic_cnt_present_flag;
};

#endif	/* H264_H */

