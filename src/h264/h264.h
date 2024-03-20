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
#include "descriptor.h"
#include "H26xNal.h"
#include <optional>

template <class T>
class H264Optional: public std::optional<T>
{
public:
	bool Decode(BitReader& r)
	{
		bool present = r.Get(1);
		CHECK(r);

		if (!present)
			this->reset();
		else
			DECODE_SUBOBJECT(this->emplace(), r);

		return !r.Error();
	}

	void DumpFields(const char* prefix) const
	{
		if(*this)
			(*this)->DumpFields(prefix);
		else
			Debug("%s<Not present>\n", prefix);
	}
};

class H264HrdParameters : public Descriptor
{
public:
	struct SchedSel : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); bit_rate_value_minus1 = ExpGolombDecoder::Decode(r);
			CHECK(r); cpb_size_value_minus1 = ExpGolombDecoder::Decode(r);
			CHECK(r); cbr_flag = r.Get(1);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(bit_rate_value_minus1, "%u");
			DUMP_FIELD(cpb_size_value_minus1, "%u");
			DUMP_FIELD(cbr_flag, "%u");
		}
		DWORD			bit_rate_value_minus1 = 0;
		DWORD			cpb_size_value_minus1 = 0;
		bool			cbr_flag = false;
	};

	bool Decode(BitReader& r)
	{
		CHECK(r); cpb_cnt_minus1 = ExpGolombDecoder::Decode(r);
		CHECK(r); bit_rate_scale = r.Get(4);
		CHECK(r); cpb_size_scale = r.Get(4);
		SchedSels.resize(cpb_cnt_minus1 + 1);
		for (size_t i = 0; i < SchedSels.size(); i++) {
			CHECK(r); DECODE_SUBOBJECT(SchedSels[i], r);
		}
		CHECK(r); initial_cpb_removal_delay_length_minus1 = r.Get(5);
		CHECK(r); cpb_removal_delay_length_minus1 = r.Get(5);
		CHECK(r); dpb_output_delay_length_minus1 = r.Get(5);
		CHECK(r); time_offset_length = r.Get(5);
		return !r.Error();
	}
public:
	void DumpFields(const char* prefix) const
	{
		DUMP_FIELD(cpb_cnt_minus1, "%u");
		DUMP_FIELD(bit_rate_scale, "%u");
		DUMP_FIELD(cpb_size_scale, "%u");
		for (size_t i = 0; i < SchedSels.size(); i++) {
			Debug("%sSchedSels[%u]=[\n", prefix, i);
			SchedSels[i].DumpFields((std::string(prefix) + "\t").c_str());
			Debug("%s]\n", prefix);
		}
		DUMP_FIELD(initial_cpb_removal_delay_length_minus1, "%u");
		DUMP_FIELD(cpb_removal_delay_length_minus1, "%u");
		DUMP_FIELD(dpb_output_delay_length_minus1, "%u");
		DUMP_FIELD(time_offset_length, "%u");
	}
private:
	DWORD			cpb_cnt_minus1 = 0;
	BYTE			bit_rate_scale = 0;
	BYTE			cpb_size_scale = 0;
	std::vector<SchedSel>	SchedSels;
	BYTE			initial_cpb_removal_delay_length_minus1 = 0;
	BYTE			cpb_removal_delay_length_minus1 = 0;
	BYTE			dpb_output_delay_length_minus1 = 0;
	BYTE			time_offset_length = 0;
};

class H264VuiParameters : public Descriptor
{
public:
	struct AspectRatioInfo : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); aspect_ratio_idc = r.Get(8);
			if (aspect_ratio_idc == Extended_SAR) {
				CHECK(r); sar_width = r.Get(16);
				CHECK(r); sar_height = r.Get(16);
			}
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(aspect_ratio_idc, "%u");
			if (aspect_ratio_idc == Extended_SAR) {
				DUMP_FIELD(sar_width, "%u");
				DUMP_FIELD(sar_height, "%u");
			}
		}
		BYTE			aspect_ratio_idc = 0;
		WORD			sar_width = 0;
		WORD			sar_height = 0;

		static const BYTE Extended_SAR = 0xFF;
	};

	struct OverscanInfo : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); overscan_appropriate_flag = r.Get(1);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(overscan_appropriate_flag, "%u");
		}
		bool			overscan_appropriate_flag = false;
	};

	struct ColourDescription : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); colour_primaries = r.Get(8);
			CHECK(r); transfer_characteristics = r.Get(8);
			CHECK(r); matrix_coefficients = r.Get(8);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(colour_primaries, "%u");
			DUMP_FIELD(transfer_characteristics, "%u");
			DUMP_FIELD(matrix_coefficients, "%u");
		}
		BYTE			colour_primaries = 0;
		BYTE			transfer_characteristics = 0;
		BYTE			matrix_coefficients = 0;
	};

	struct VideoSignalType : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); video_format = r.Get(3);
			CHECK(r); video_full_range_flag = r.Get(1);
			CHECK(r); DECODE_SUBOBJECT(colour_description, r);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(video_format, "%u");
			DUMP_FIELD(video_full_range_flag, "%u");
			DUMP_SUBOBJECT(colour_description);
		}
		BYTE				video_format = 0;
		BYTE				video_full_range_flag = 0;
		H264Optional<ColourDescription>	colour_description;
	};

	struct ChromaSampleLocInfo : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); type_top_field = ExpGolombDecoder::Decode(r);
			CHECK(r); type_bottom_field = ExpGolombDecoder::Decode(r);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(type_top_field, "%u");
			DUMP_FIELD(type_bottom_field, "%u");
		}
		DWORD			type_top_field = 0;
		DWORD			type_bottom_field = 0;
	};

	struct TimingInfo : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); num_units_in_tick = r.Get(32);
			CHECK(r); time_scale = r.Get(32);
			CHECK(r); fixed_frame_rate_flag = r.Get(1);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(num_units_in_tick, "%u");
			DUMP_FIELD(time_scale, "%u");
			DUMP_FIELD(fixed_frame_rate_flag, "%u");
		}
		DWORD			num_units_in_tick = 0;
		DWORD			time_scale = 0;
		bool			fixed_frame_rate_flag = false;
	};

	struct BitstreamRestriction : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); motion_vectors_over_pic_boundaries_flag = r.Get(1);
			CHECK(r); max_bytes_per_pic_denom = ExpGolombDecoder::Decode(r);
			CHECK(r); max_bits_per_mb_denom = ExpGolombDecoder::Decode(r);
			CHECK(r); log2_max_mv_length_horizontal = ExpGolombDecoder::Decode(r);
			CHECK(r); log2_max_mv_length_vertical = ExpGolombDecoder::Decode(r);
			CHECK(r); max_num_reorder_frames = ExpGolombDecoder::Decode(r);
			CHECK(r); max_dec_frame_buffering = ExpGolombDecoder::Decode(r);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(motion_vectors_over_pic_boundaries_flag, "%u");
			DUMP_FIELD(max_bytes_per_pic_denom, "%u");
			DUMP_FIELD(max_bits_per_mb_denom, "%u");
			DUMP_FIELD(log2_max_mv_length_horizontal, "%u");
			DUMP_FIELD(log2_max_mv_length_vertical, "%u");
			DUMP_FIELD(max_num_reorder_frames, "%u");
			DUMP_FIELD(max_dec_frame_buffering, "%u");
		}
		bool			motion_vectors_over_pic_boundaries_flag = false;
		DWORD			max_bytes_per_pic_denom = 0;
		DWORD			max_bits_per_mb_denom = 0;
		DWORD			log2_max_mv_length_horizontal = 0;
		DWORD			log2_max_mv_length_vertical = 0;
		DWORD			max_num_reorder_frames = 0;
		DWORD			max_dec_frame_buffering = 0;
	};

	bool Decode(BitReader& r)
	{
		CHECK(r); DECODE_SUBOBJECT(aspect_ratio_info, r);
		CHECK(r); DECODE_SUBOBJECT(overscan_info, r);
		CHECK(r); DECODE_SUBOBJECT(video_signal_type, r);
		CHECK(r); DECODE_SUBOBJECT(chroma_loc_info, r);
		CHECK(r); DECODE_SUBOBJECT(timing_info, r);
		CHECK(r); DECODE_SUBOBJECT(nal_hrd_parameters, r);
		CHECK(r); DECODE_SUBOBJECT(vcl_hrd_parameters, r);
		if (nal_hrd_parameters || vcl_hrd_parameters)
		{
			CHECK(r); low_delay_hrd_flag = r.Get(1);
		}
		CHECK(r); pic_struct_present_flag = r.Get(1);
		CHECK(r); DECODE_SUBOBJECT(bitstream_restriction, r);
		return !r.Error();
	}
public:
	void DumpFields(const char* prefix) const
	{
		DUMP_SUBOBJECT(aspect_ratio_info);
		DUMP_SUBOBJECT(overscan_info);
		DUMP_SUBOBJECT(video_signal_type);
		DUMP_SUBOBJECT(chroma_loc_info);
		DUMP_SUBOBJECT(timing_info);
		DUMP_SUBOBJECT(nal_hrd_parameters);
		DUMP_SUBOBJECT(vcl_hrd_parameters);
		if (nal_hrd_parameters || vcl_hrd_parameters)
			DUMP_FIELD(low_delay_hrd_flag, "%u");
		DUMP_FIELD(pic_struct_present_flag, "%u");
		DUMP_SUBOBJECT(bitstream_restriction);
	}
private:
	H264Optional<AspectRatioInfo>		aspect_ratio_info;
	H264Optional<OverscanInfo>		overscan_info;
	H264Optional<VideoSignalType>		video_signal_type;
	H264Optional<ChromaSampleLocInfo>	chroma_loc_info;
	H264Optional<TimingInfo>		timing_info;
	H264Optional<H264HrdParameters>		nal_hrd_parameters;
	H264Optional<H264HrdParameters>		vcl_hrd_parameters;
	bool					low_delay_hrd_flag = false;
	bool					pic_struct_present_flag = false;
	H264Optional<BitstreamRestriction> 	bitstream_restriction;
};

class H264SeqParameterSet : public Descriptor
{
public:
	struct FrameCrop : public Descriptor
	{
		bool Decode(BitReader& r)
		{
			CHECK(r); left_offset = ExpGolombDecoder::Decode(r);
			CHECK(r); right_offset = ExpGolombDecoder::Decode(r);
			CHECK(r); top_offset = ExpGolombDecoder::Decode(r);
			CHECK(r); bottom_offset = ExpGolombDecoder::Decode(r);
			return !r.Error();
		}
		void DumpFields(const char* prefix) const
		{
			DUMP_FIELD(left_offset, "%u");
			DUMP_FIELD(right_offset, "%u");
			DUMP_FIELD(top_offset, "%u");
			DUMP_FIELD(bottom_offset, "%u");
		}
		DWORD			left_offset = 0;
		DWORD			right_offset = 0;
		DWORD			top_offset = 0;
		DWORD			bottom_offset = 0;
	};

	bool Decode(const BYTE* buffer,DWORD bufferSize)
	{
		//SHould be done otherway, like modifying the BitReader to escape the input NAL, but anyway.. duplicate memory
		BYTE *aux = (BYTE*)malloc(bufferSize);
		//Escape
		DWORD len = NalUnescapeRbsp(aux,buffer,bufferSize);
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
			CHECK(r); [[maybe_unused]] auto chroma_format_idc = ExpGolombDecoder::Decode(r);
			if( chroma_format_idc == 3 )
			{
				CHECK(r); separate_colour_plane_flag = r.Get(1);
			}
			CHECK(r); [[maybe_unused]] auto bit_depth_luma_minus8 = ExpGolombDecoder::Decode(r);
			CHECK(r); [[maybe_unused]] auto bit_depth_chroma_minus8 = ExpGolombDecoder::Decode(r);
			CHECK(r); [[maybe_unused]] auto qpprime_y_zero_transform_bypass_flag = r.Get(1);
			CHECK(r); [[maybe_unused]] auto seq_scaling_matrix_present_flag = r.Get(1);
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
		CHECK(r); DECODE_SUBOBJECT(frame_crop, r);
		CHECK(r); DECODE_SUBOBJECT(vui_parameters, r);
		//Free memory
		free(aux);
		//OK
		return !r.Error();
	}
public:
	FrameCrop GetFrameCrop() const	{ return frame_crop.value_or(FrameCrop()); }
	DWORD GetWidth() const	{ return ((pic_width_in_mbs_minus1 +1)*16) - GetFrameCrop().right_offset *2 - GetFrameCrop().left_offset *2; }
	DWORD GetHeight() const	{ return ((2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 +1) * 16) - GetFrameCrop().bottom_offset*2 - GetFrameCrop().top_offset*2; }

	bool GetSeparateColourPlaneFlag() const { return separate_colour_plane_flag; }
	bool GetFrameMbsOnlyFlag() const { return frame_mbs_only_flag; }
	BYTE GetLog2MaxFrameNumMinus4() const { return log2_max_frame_num_minus4; }
	
	void DumpFields(const char* prefix) const
	{
		DUMP_FIELD(profile_idc, "%.2x");
		DUMP_FIELD(constraint_set0_flag, "%u");
		DUMP_FIELD(constraint_set1_flag, "%u");
		DUMP_FIELD(constraint_set2_flag, "%u");
		DUMP_FIELD(reserved_zero_5bits, "%u");
		DUMP_FIELD(level_idc, "%.2x");
		DUMP_FIELD(seq_parameter_set_id, "%u");
		DUMP_FIELD(log2_max_frame_num_minus4, "%u");
		DUMP_FIELD(pic_order_cnt_type, "%u");
		DUMP_FIELD(log2_max_pic_order_cnt_lsb_minus4, "%u");
		DUMP_FIELD(delta_pic_order_always_zero_flag, "%u");
		DUMP_FIELD(offset_for_non_ref_pic, "%d");
		DUMP_FIELD(offset_for_top_to_bottom_field, "%d");
		DUMP_FIELD(num_ref_frames_in_pic_order_cnt_cycle, "%u");
		DUMP_FIELD(num_ref_frames, "%u");
		DUMP_FIELD(gaps_in_frame_num_value_allowed_flag, "%u");
		DUMP_FIELD(pic_width_in_mbs_minus1, "%u");
		DUMP_FIELD(pic_height_in_map_units_minus1, "%u");
		DUMP_FIELD(frame_mbs_only_flag, "%u");
		DUMP_FIELD(mb_adaptive_frame_field_flag, "%u");
		DUMP_FIELD(direct_8x8_inference_flag, "%u");
		DUMP_SUBOBJECT(frame_crop);
		DUMP_SUBOBJECT(vui_parameters);
		DUMP_FIELD(separate_colour_plane_flag, "%u");
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
	H264Optional<FrameCrop>	frame_crop;
	H264Optional<H264VuiParameters>	vui_parameters;
	bool			separate_colour_plane_flag = 0;
};

class H264PictureParameterSet : public Descriptor
{
public:
	bool Decode(const BYTE* buffer,DWORD bufferSize)
	{
		//SHould be done otherway, like modifying the BitReader to escape the input NAL, but anyway.. duplicate memory
		BYTE *aux = (BYTE*)malloc(bufferSize);
		//Escape
		DWORD len = NalUnescapeRbsp(aux,buffer,bufferSize);
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
public:
	void DumpFields(const char* prefix) const
	{
		DUMP_FIELD(pic_parameter_set_id, "%u");
		DUMP_FIELD(seq_parameter_set_id, "%u");
		DUMP_FIELD(entropy_coding_mode_flag, "%u");
		DUMP_FIELD(pic_order_present_flag, "%u");
		DUMP_FIELD(num_slice_groups_minus1, "%d");
		DUMP_FIELD(slice_group_map_type, "%u");
		DUMP_FIELD(slice_group_change_direction_flag, "%u");
		DUMP_FIELD(slice_group_change_rate_minus1, "%u");
		DUMP_FIELD(pic_size_in_map_units_minus1, "%d");
		DUMP_FIELD(num_ref_idx_l0_active_minus1, "%d");
		DUMP_FIELD(num_ref_idx_l1_active_minus1, "%d");
		DUMP_FIELD(weighted_pred_flag, "%u");
		DUMP_FIELD(weighted_bipred_idc, "%u");
		DUMP_FIELD(pic_init_qp_minus26, "%d");
		DUMP_FIELD(pic_init_qs_minus26, "%d");
		DUMP_FIELD(chroma_qp_index_offset, "%d");
		DUMP_FIELD(deblocking_filter_control_present_flag, "%u");
		DUMP_FIELD(constrained_intra_pred_flag, "%u");
		DUMP_FIELD(redundant_pic_cnt_present_flag, "%u");
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

class H264SliceHeader : public Descriptor
{
public:

	bool Decode(const BYTE* buffer, DWORD bufferSize, const H264SeqParameterSet& sps)
	{	
		BitReader r(buffer, bufferSize);
		
		first_mb_in_slice = ExpGolombDecoder::Decode(r); CHECK(r);
		slice_type = ExpGolombDecoder::Decode(r); CHECK(r);
		pic_parameter_set_id = ExpGolombDecoder::Decode(r); CHECK(r);
		
		if (sps.GetSeparateColourPlaneFlag())
		{
			colour_plane_id = r.Get(2); CHECK(r);
		}
		
		frame_num = r.Get(sps.GetLog2MaxFrameNumMinus4() + 4); CHECK(r);
		
		if (!sps.GetFrameMbsOnlyFlag())
		{
			field_pic_flag = r.Get(1); CHECK(r);
			if (field_pic_flag)
			{
				bottom_field_flag = r.Get(1); CHECK(r);
			}
		}
		
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
	
	void DumpFields(const char* prefix) const
	{
		DUMP_FIELD(first_mb_in_slice, "%u");
		DUMP_FIELD(slice_type, "%u");
		DUMP_FIELD(pic_parameter_set_id, "%u");
		DUMP_FIELD(colour_plane_id, "%u");
		DUMP_FIELD(field_pic_flag, "%d");
		DUMP_FIELD(bottom_field_flag, "%d");
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

#include "descriptor_undef.h"

#endif	/* H264_H */

