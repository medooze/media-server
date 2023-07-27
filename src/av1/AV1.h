#ifndef AV1_H
#define AV1_H
#include "config.h"
#include "log.h"
#include "bitstream.h"

class AV1CodecConfig
{
public:
	DWORD Serialize(BYTE* buffer, DWORD length) const
	{
		if (length < GetSize())
			return 0;

		int l = 0;

		buffer[l++] = profile;
		buffer[l++] = level;
		buffer[l++] = (bitDepth << 4) | (chromaSubsampling << 1) | (videoFullRangeFlag ? 1 : 0);
		buffer[l++] = colorPrimaries;
		buffer[l++] = transferCharacteristics;
		buffer[l++] = matrixCoefficients;
		//add initialization data length
		set2(buffer, l, initializationData.size());
		//Inc leght
		l += 2;
		//Copy it
		memcpy(buffer + l, initializationData.data(), initializationData.size());
		//Inc length
		l += initializationData.size();
		//Done
		return l;
	}

	DWORD GetSize() const
	{
		return 8 + initializationData.size();
	}
private:
	uint8_t profile = 0;
	uint8_t level = 10;
	uint8_t bitDepth = 8;
	uint8_t chromaSubsampling = 0; //CHROMA_420
	bool videoFullRangeFlag = false;
	uint8_t colorPrimaries = 2; //Unspecified
	uint8_t transferCharacteristics = 2; //Unspecified
	uint8_t matrixCoefficients = 2; //Unspecified
	std::vector<uint8_t> initializationData;
};

struct SequenceHeaderObu
{
	uint8_t seq_profile;
	bool	still_picture;
	bool	reduced_still_picture_header;
	bool	timing_info_present_flag;
	bool	decoder_model_info_present_flag;
	bool	initial_display_delay_present_flag;
	uint8_t	operating_points_cnt_minus_1;
	std::map<uint8_t,uint16_t> operating_point_idc;
	std::map<uint8_t,uint8_t> seq_level_idx;
	std::map<uint8_t,uint8_t> seq_tier;
	std::map<uint8_t,uint8_t> decoder_model_present_for_this_op;
	std::map<uint8_t,uint8_t> initial_display_delay_present_for_this_op;
	std::map<uint8_t,uint8_t> initial_display_delay_minus_1;
	uint32_t max_frame_width_minus_1;
	uint32_t max_frame_height_minus_1;
	bool	frame_id_numbers_present_flag;
	uint8_t delta_frame_id_length_minus_2;
	uint8_t additional_frame_id_length_minus_1;
	bool	use_128x128_superblock;
	bool	enable_filter_intra;
	bool	enable_intra_edge_filter;
	bool	enable_interintra_compound;
	bool	enable_masked_compound;
	bool	enable_warped_motion;
	bool	enable_dual_filter;
	bool	enable_order_hint;
	bool	enable_jnt_comp;
	bool	enable_ref_frame_mvs;
	uint8_t	seq_force_screen_content_tools;
	uint8_t	seq_force_integer_mv;
	bool	enable_superres;
	bool	enable_cdef;
	bool	enable_restoration;
	bool	film_grain_params_present;

	bool	seq_choose_screen_content_tools;
	bool	seq_choose_integer_mv;
	uint8_t order_hint_bits_minus_1;

	std::map<uint8_t,uint32_t> decoder_buffer_delay;
	std::map<uint8_t,uint32_t> encoder_buffer_delay;
	std::map<uint8_t,bool>     low_delay_mode_flag;

	uint32_t num_units_in_display_tick;
	uint32_t time_scale;
	bool	 equal_picture_interval;
	uint32_t num_ticks_per_picture_minus_1;

	uint8_t	buffer_delay_length_minus_1;
	uint32_t num_units_in_decoding_tick;
	uint8_t	buffer_removal_time_length_minus_1;
	uint8_t	frame_presentation_time_length_minus_1;

	static constexpr uint8_t SELECT_INTEGER_MV = 2;
	static constexpr uint8_t SELECT_SCREEN_CONTENT_TOOLS = 2;
	

	bool Parse(const BYTE* buffer, DWORD length)
	{
		BitReader r(buffer, length);

		seq_profile = r.Get(3);
		still_picture = r.Get(1);
		reduced_still_picture_header = r.Get(1);

		if (reduced_still_picture_header) 
		{
			timing_info_present_flag = 0;
			decoder_model_info_present_flag = 0;
			initial_display_delay_present_flag = 0;
			operating_points_cnt_minus_1 = 0;
			operating_point_idc[0] = 0;
			seq_level_idx[0] = r.Get(5);
			seq_tier[0] = 0;
			decoder_model_present_for_this_op[0] = 0;
			initial_display_delay_present_for_this_op[0] = 0;
		} else {
			timing_info_present_flag = r.Get(1);
			if (timing_info_present_flag) 
			{
				//timing_info()
				num_units_in_display_tick = r.Get(32);
				time_scale = r.Get(32);
				equal_picture_interval = r.Get(1);
				if (equal_picture_interval)
					num_ticks_per_picture_minus_1 = r.GetVariableLengthUnsigned();

				decoder_model_info_present_flag = r.Get(1);
				if (decoder_model_info_present_flag) 
				{
					//decoder_model_info()
					buffer_delay_length_minus_1 = r.Get(5);
					num_units_in_decoding_tick = r.Get(32);
					buffer_removal_time_length_minus_1 = r.Get(5);
					frame_presentation_time_length_minus_1 = r.Get(5);
				}
			} else {
				decoder_model_info_present_flag = 0;
			}
			initial_display_delay_present_flag = r.Get(1);
			operating_points_cnt_minus_1 = r.Get(5);

			for (int i = 0; i <= operating_points_cnt_minus_1; i++) 
			{
				operating_point_idc[i] = r.Get(12);
				seq_level_idx[i] = r.Get(5);
				if (seq_level_idx[i] > 7) {
					seq_tier[i] = r.Get(1);
				} else {
					seq_tier[i] = 0;
				}
				if (decoder_model_info_present_flag) 
				{
					decoder_model_present_for_this_op[i] = r.Get(1);
					if (decoder_model_present_for_this_op[i]) {
						int n = buffer_delay_length_minus_1 + 1;
						decoder_buffer_delay[i] = r.Get(n);
						encoder_buffer_delay[i] = r.Get(n);
						low_delay_mode_flag[i] = r.Get(1);
					}
				} else {
					decoder_model_present_for_this_op[i] = 0;
				}
				if (initial_display_delay_present_flag) 
				{
					initial_display_delay_present_for_this_op[i] = r.Get(1);
					if (initial_display_delay_present_for_this_op[i]) {
						initial_display_delay_minus_1[i] = r.Get(4);
					}
				}
			}
		}

		auto frame_width_bits_minus_1 = r.Get(4);
		auto frame_height_bits_minus_1 = r.Get(4);
		max_frame_width_minus_1 = r.Get(frame_width_bits_minus_1 + 1);
		max_frame_height_minus_1 = r.Get(frame_height_bits_minus_1 + 1);
		if (reduced_still_picture_header)
			frame_id_numbers_present_flag = 0;
		else
			frame_id_numbers_present_flag = r.Get(1);
		if (frame_id_numbers_present_flag) 
		{
			delta_frame_id_length_minus_2 = r.Get(4);
			additional_frame_id_length_minus_1 = r.Get(3);
		}
		use_128x128_superblock = r.Get(1);
		enable_filter_intra = r.Get(1);
		enable_intra_edge_filter = r.Get(1);
		if (reduced_still_picture_header) 
		{
			enable_interintra_compound = 0;
			enable_masked_compound = 0;
			enable_warped_motion = 0;
			enable_dual_filter = 0;
			enable_order_hint = 0;
			enable_jnt_comp = 0;
			enable_ref_frame_mvs = 0;
			seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
			seq_force_integer_mv = SELECT_INTEGER_MV;
		} else {
			enable_interintra_compound = r.Get(1);
			enable_masked_compound = r.Get(1);
			enable_warped_motion = r.Get(1);
			enable_dual_filter = r.Get(1);
			enable_order_hint = r.Get(1);
			if (enable_order_hint) {
				enable_jnt_comp = r.Get(1);
				enable_ref_frame_mvs = r.Get(1);
			} else {
				enable_jnt_comp = 0;
				enable_ref_frame_mvs = 0;
			}
			seq_choose_screen_content_tools = r.Get(1);
			if (seq_choose_screen_content_tools) {
				seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
			} else {
				seq_force_screen_content_tools = r.Get(1);
			}
			if (seq_force_screen_content_tools > 0) 
			{
				seq_choose_integer_mv = r.Get(1);
				if (seq_choose_integer_mv) 
				{
					seq_force_integer_mv = SELECT_INTEGER_MV;
				} else {
					seq_force_integer_mv = r.Get(1);
				}
			} else {
				seq_force_integer_mv = SELECT_INTEGER_MV;
			}
			if (enable_order_hint) 
				order_hint_bits_minus_1 = r.Get(3);
		}
		enable_superres = r.Get(1);
		enable_cdef = r.Get(1);
		enable_restoration = r.Get(1);
		//TODO color_config();
		//film_grain_params_present = r.Get(1);

		return 1;
	}


};

struct RtpAv1AggreationHeader
{
	uint8_t Reserved : 3;
	uint8_t N : 1;	
	uint8_t W : 2;
	uint8_t Y : 1;	
	uint8_t Z : 1;
};

#endif /* AV1_H */

