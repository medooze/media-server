#ifndef H264_H
#define	H264_H
#include "config.h"
#include "math.h"
#include "H26xNal.h"

class H264SeqParameterSet
{
public:
	bool Decode(const BYTE* buffer,DWORD bufferSize)
	{
		RbspReader reader(buffer, bufferSize);
		RbspBitReader r(reader);

		for (int i = 0; i < bufferSize; i++)
		{
			printf("0x%x, ", buffer[i]);
		}

		try {
			//Read SPS
			data.profile_idc = r.Get(8);
			data.constraint_set0_flag = r.Get(1);
			data.constraint_set1_flag = r.Get(1);
			data.constraint_set2_flag = r.Get(1);
			data.reserved_zero_5bits  = r.Get(5);
			data.level_idc = r.Get(8);
			data.seq_parameter_set_id = r.GetExpGolomb();
		
			//Check profile
			if(data.profile_idc==100 || data.profile_idc==110 || data.profile_idc==122 || data.profile_idc==244 || data.profile_idc==44 || 
				data.profile_idc==83 || data.profile_idc==86 || data.profile_idc==118 || data.profile_idc==128 || data.profile_idc==138 || 
				data.profile_idc==139 || data.profile_idc==134 || data.profile_idc==135)
			{
				data.chroma_format_idc = r.GetExpGolomb();

				if( data.chroma_format_idc == 3 )
				{
					data.separate_colour_plane_flag = r.Get(1);
				}

				[[maybe_unused]] auto bit_depth_luma_minus8			= r.GetExpGolomb();
				[[maybe_unused]] auto bit_depth_chroma_minus8			= r.GetExpGolomb();
				[[maybe_unused]] auto qpprime_y_zero_transform_bypass_flag	= r.Get(1);
				[[maybe_unused]] auto seq_scaling_matrix_present_flag		= r.Get(1);

				if( seq_scaling_matrix_present_flag )
				{	
					for(uint32_t i = 0; i < (( data.chroma_format_idc != 3 ) ? 8 : 12 ); i++ )
					{
						auto seq_scaling_list_present_flag = r.Get(1);
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

			data.log2_max_frame_num_minus4	= r.GetExpGolomb();
			data.pic_order_cnt_type		= r.GetExpGolomb();

			if( data.pic_order_cnt_type == 0 )
			{
				data.log2_max_pic_order_cnt_lsb_minus4 = r.GetExpGolomb();
			} else if( data.pic_order_cnt_type == 1 ) {
				data.delta_pic_order_always_zero_flag	= r.Get(1);
				data.offset_for_non_ref_pic			= r.GetExpGolomb(); 
				data.offset_for_top_to_bottom_field		= r.GetExpGolomb(); 
				data.num_ref_frames_in_pic_order_cnt_cycle	= r.GetExpGolomb();

				for( DWORD i = 0; i < data.num_ref_frames_in_pic_order_cnt_cycle; i++ )
				{
					data.offset_for_ref_frame.assign(i,r.GetExpGolomb());
				}
			}

			data.num_ref_frames				= r.GetExpGolomb();
			data.gaps_in_frame_num_value_allowed_flag	= r.Get(1);
			data.pic_width_in_mbs_minus1			= r.GetExpGolomb();
			data.pic_height_in_map_units_minus1		= r.GetExpGolomb();
			data.frame_mbs_only_flag			= r.Get(1);

			if (!data.frame_mbs_only_flag)
			{
				data.mb_adaptive_frame_field_flag	= r.Get(1);
			}

			data.direct_8x8_inference_flag		= r.Get(1);
			data.frame_cropping_flag	= r.Get(1);
			
			if (data.frame_cropping_flag)
			{
				data.frame_crop_left_offset		= r.GetExpGolomb();
				data.frame_crop_right_offset		= r.GetExpGolomb();
				data.frame_crop_top_offset		= r.GetExpGolomb();
				data.frame_crop_bottom_offset	= r.GetExpGolomb();
			}

			[[maybe_unused]] auto vui_parameters_present_flag = r.Get(1);
			
			//TODO: parse vui
		}
		catch (std::exception &e) 
		{
			Warning("-H264SeqParameterSet::Decoder() Exception\n");
			//Error
			return false;
		}
		//OK
		return true;
	}

private:
	struct sps_data{
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
		int			    offset_for_non_ref_pic = 0;
		int			    offset_for_top_to_bottom_field = 0;
		BYTE			num_ref_frames_in_pic_order_cnt_cycle = 0;
		std::vector<int>	offset_for_ref_frame;
		DWORD			num_ref_frames = 0;
		DWORD			chroma_format_idc = 0;
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
		bool			separate_colour_plane_flag = 0;
	}data;

public:
	DWORD GetWidth()	{ return ((data.pic_width_in_mbs_minus1 +1)*16) - data.frame_crop_right_offset *2 - data.frame_crop_left_offset *2; }
	DWORD GetHeight()	{ return ((2 - data.frame_mbs_only_flag)* (data.pic_height_in_map_units_minus1 +1) * 16) - data.frame_crop_bottom_offset*2 - data.frame_crop_top_offset*2; }

	bool GetSeparateColourPlaneFlag() const { return data.separate_colour_plane_flag; }
	bool GetFrameMbsOnlyFlag() const { return data.frame_mbs_only_flag; }
	BYTE GetLog2MaxFrameNumMinus4() const { return data.log2_max_frame_num_minus4; }

	sps_data GetSPSData() const {return data;}
	
	void Dump() const
	{
		Debug("[H264SeqParameterSet \n");
		Debug("\tprofile_idc=%.2x\n",				data.profile_idc);
		Debug("\tconstraint_set0_flag=%u\n",			data.constraint_set0_flag);
		Debug("\tconstraint_set1_flag=%u\n",			data.constraint_set1_flag);
		Debug("\tconstraint_set2_flag=%u\n",			data.constraint_set2_flag);
		Debug("\treserved_zero_5bits =%u\n",			data.reserved_zero_5bits );
		Debug("\tlevel_idc=%.2x\n",				data.level_idc);
		Debug("\tseq_parameter_set_id=%u\n",			data.seq_parameter_set_id);
		Debug("\tlog2_max_frame_num_minus4=%u\n",		data.log2_max_frame_num_minus4);
		Debug("\tpic_order_cnt_type=%u\n",			data.pic_order_cnt_type);
		Debug("\tlog2_max_pic_order_cnt_lsb_minus4=%u\n",	data.log2_max_pic_order_cnt_lsb_minus4);
		Debug("\t delta_pic_order_always_zero_flag=%u\n",	data.delta_pic_order_always_zero_flag);
		Debug("\toffset_for_non_ref_pic=%d\n",			data.offset_for_non_ref_pic);
		Debug("\toffset_for_top_to_bottom_field=%d\n",		data.offset_for_top_to_bottom_field);
		Debug("\tnum_ref_frames_in_pic_order_cnt_cycle=%u\n",	data.num_ref_frames_in_pic_order_cnt_cycle);
		Debug("\tnum_ref_frames=%u\n",				data.num_ref_frames);
		Debug("\tgaps_in_frame_num_value_allowed_flag=%u\n",	data.gaps_in_frame_num_value_allowed_flag);
		Debug("\tpic_width_in_mbs_minus1=%u\n",			data.pic_width_in_mbs_minus1);
		Debug("\tpic_height_in_map_units_minus1=%u\n",		data.pic_height_in_map_units_minus1);
		Debug("\tframe_mbs_only_flag=%u\n",			data.frame_mbs_only_flag);
		Debug("\tmb_adaptive_frame_field_flag=%u\n",		data.mb_adaptive_frame_field_flag);
		Debug("\tdirect_8x8_inference_flag=%u\n",		data.direct_8x8_inference_flag);
		Debug("\tframe_cropping_flag=%u\n",			data.frame_cropping_flag);
		Debug("\tframe_crop_left_offset=%u\n",			data.frame_crop_left_offset);
		Debug("\tframe_crop_right_offset=%u\n",			data.frame_crop_right_offset);
		Debug("\tframe_crop_top_offset=%u\n",			data.frame_crop_top_offset);
		Debug("\tframe_crop_bottom_offset=%u\n",		data.frame_crop_bottom_offset);
		Debug("\tseparate_colour_plane_flag=%u\n",		data.separate_colour_plane_flag);
		Debug("/]\n");
	}
	
};

class H264PictureParameterSet
{
public:
	bool Decode(const BYTE* buffer,DWORD bufferSize)
	{
		RbspReader reader(buffer, bufferSize);
		RbspBitReader r(reader);

		try
		{
			//Read SQS
			data.pic_parameter_set_id	 = r.GetExpGolomb();
			data.seq_parameter_set_id	 = r.GetExpGolomb();
			data.entropy_coding_mode_flag = r.Get(1);
			data.pic_order_present_flag	 = r.Get(1);
			data.num_slice_groups_minus1	 = r.GetExpGolomb();

			if( data.num_slice_groups_minus1 > 0 )
			{
				data.slice_group_map_type = r.GetExpGolomb();

				if( data.slice_group_map_type == 0 )
				{
					for( int iGroup = 0; iGroup <= data.num_slice_groups_minus1; iGroup++ )
					{
						data.run_length_minus1.assign(iGroup,r.GetExpGolomb());
					}
				}
				else if( data.slice_group_map_type == 2 ) 
				{
					for( int iGroup = 0; iGroup < data.num_slice_groups_minus1; iGroup++ )
					{
						data.top_left.assign(iGroup,r.GetExpGolomb());
						data.bottom_right.assign(iGroup,r.GetExpGolomb());
					}
				}
				else if( data.slice_group_map_type == 3 || data.slice_group_map_type ==  4 || data.slice_group_map_type == 5 )
				{
					data.slice_group_change_direction_flag = r.Get(1);
					data.slice_group_change_rate_minus1 = r.GetExpGolomb();
				}
				else if( data.slice_group_map_type == 6 ) 
				{
					data.pic_size_in_map_units_minus1 = r.GetExpGolomb();
					for( int i = 0; i <= data.pic_size_in_map_units_minus1; i++ )
					{
						data.slice_group_id.assign(i,r.Get(ceil(log2(data.num_slice_groups_minus1+1))));
					}
				}
			}

			data.num_ref_idx_l0_active_minus1		= r.GetExpGolomb();
			data.num_ref_idx_l1_active_minus1		= r.GetExpGolomb();
			data.weighted_pred_flag			= r.Get(1);
			data.weighted_bipred_idc			= r.Get(2);
			data.pic_init_qp_minus26			= r.GetExpGolombSigned(); //Signed
			data.pic_init_qs_minus26			= r.GetExpGolombSigned(); //Signed
			data.chroma_qp_index_offset			= r.GetExpGolombSigned(); //Signed
			data.deblocking_filter_control_present_flag	= r.Get(1);
			data.constrained_intra_pred_flag		= r.Get(1);
			data.redundant_pic_cnt_present_flag		= r.Get(1);
		}
		catch (std::exception& e)
		{
			Warning("-H264PictureParameterSet::Decoder() Exception\n");
			//Error
			return false;
		}
		//OK
		return true;
	}
private:
	struct pps_data{
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
	}data;

public:
	pps_data GetPPSData() const {return data;}
	void Dump() const
	{
		Debug("[H264PictureParameterSet \n");
		Debug("\tpic_parameter_set_id=%u\n",			data.pic_parameter_set_id);
		Debug("\tseq_parameter_set_id=%u\n",			data.seq_parameter_set_id);
		Debug("\tentropy_coding_mode_flag=%u\n",		data.entropy_coding_mode_flag);
		Debug("\tpic_order_present_flag=%u\n",			data.pic_order_present_flag);
		Debug("\tnum_slice_groups_minus1=%d\n",			data.num_slice_groups_minus1);
		Debug("\tslice_group_map_type=%u\n",			data.slice_group_map_type);
		Debug("\tslice_group_change_direction_flag=%u\n",	data.slice_group_change_direction_flag);
		Debug("\tslice_group_change_rate_minus1=%u\n",		data.slice_group_change_rate_minus1);
		Debug("\tpic_size_in_map_units_minus1=%d\n",		data.pic_size_in_map_units_minus1);
		Debug("\tnum_ref_idx_l0_active_minus1=%d\n",		data.num_ref_idx_l0_active_minus1);
		Debug("\tnum_ref_idx_l1_active_minus1=%d\n",		data.num_ref_idx_l1_active_minus1);
		Debug("\tweighted_pred_flag=%u\n",			data.weighted_pred_flag);
		Debug("\tweighted_bipred_idc=%u\n",			data.weighted_bipred_idc);
		Debug("\tpic_init_qp_minus26=%d\n",			data.pic_init_qp_minus26);
		Debug("\tpic_init_qs_minus26=%d\n",			data.pic_init_qs_minus26);
		Debug("\tchroma_qp_index_offset=%d\n",			data.chroma_qp_index_offset);
		Debug("\tdeblocking_filter_control_present_flag=%u\n",	data.deblocking_filter_control_present_flag);
		Debug("\tconstrained_intra_pred_flag=%u\n",		data.constrained_intra_pred_flag);
		Debug("\tredundant_pic_cnt_present_flag=%u\n",		data.redundant_pic_cnt_present_flag);
		Debug("/]\n");
	}
};

class H264SliceHeader
{
public:

	bool Decode(const BYTE* buffer, DWORD bufferSize, const H264SeqParameterSet& sps)
	{	
		RbspReader reader(buffer, bufferSize);
		RbspBitReader r(reader);
		
		try
		{
			first_mb_in_slice	= r.GetExpGolomb();
			slice_type		= r.GetExpGolomb();
			pic_parameter_set_id	= r.GetExpGolomb();
		
			if (sps.GetSeparateColourPlaneFlag())
			{
				colour_plane_id = r.Get(2);
			}
		
			frame_num = r.Get(sps.GetLog2MaxFrameNumMinus4() + 4);
		
			if (!sps.GetFrameMbsOnlyFlag())
			{
				field_pic_flag = r.Get(1);
				if (field_pic_flag)
				{
					bottom_field_flag = r.Get(1);
				}
			}
		}
		catch (std::exception& e)
		{
			Warning("-H264SliceHeader::Decoder() Exception\n");
			//Error
			return false;
		}
		//OK
		return true;
	}
	
	inline uint32_t GetFirstMbInSlice() const
	{
		return first_mb_in_slice;
	}
	
	inline uint32_t GetSliceType() const
	{
		return slice_type;
	}
	
	inline uint32_t GetPicPpsId() const
	{
		return pic_parameter_set_id;
	}
	
	inline uint32_t GetColourPlaneId() const
	{
		return colour_plane_id;
	}
	
	inline uint32_t GetFrameNum() const
	{
		return frame_num;
	}
	
	inline bool GetFieldPicFlag() const
	{
		return field_pic_flag;
	}
	
	inline bool GetBottomFieldFlag() const
	{
		return bottom_field_flag;
	}
	
	void Dump() const
	{
		Debug("[H264SliceHeader \n");
		Debug("\tfirstMbInSlice=%u\n", first_mb_in_slice);
		Debug("\tslice_type=%u\n", slice_type);
		Debug("\tpic_parameter_set_id=%u\n", pic_parameter_set_id);
		Debug("\tcolour_plane_id=%u\n", colour_plane_id);
		Debug("\field_pic_flag=%d\n", field_pic_flag);
		Debug("\tbottom_field_flag=%d\n", bottom_field_flag);
		Debug("/]\n");
	}

private:
	uint32_t first_mb_in_slice = 0;
	uint32_t slice_type = 0;
	uint32_t pic_parameter_set_id = 0;
	uint8_t colour_plane_id = 0;
	uint8_t frame_num = 0;
	bool field_pic_flag = false;
	bool bottom_field_flag = false;
};

#undef CHECK

#endif	/* H264_H */

