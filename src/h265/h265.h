/* 
 * File:   h265.h
 * Author: Zita	Liao
 *
 * Created on 14 June 2023,	10:19
 */

#ifndef	H265_H
#define	H265_H
#include "config.h"
#include "math.h"
#include "bitstream.h"
#include "log.h"
#include <array>

#define	CHECK(r) if(r.Error()) return false;

/**
 * Table 7-1 – NAL unit	type codes and NAL unit	type classes in	T-REC-H.265-201802
 * rfc7798:	RTP	payload	for	HEVC/H.265
 */

enum HEVC_RTP_NALU_Type	{
	TRAIL_N		   = 0,
	TRAIL_R		   = 1,
	TSA_N		   = 2,
	TSA_R		   = 3,
	STSA_N		   = 4,
	STSA_R		   = 5,
	RADL_N		   = 6,
	RADL_R		   = 7,
	RASL_N		   = 8,
	RASL_R		   = 9,
	VCL_N10		   = 10,
	VCL_R11		   = 11,
	VCL_N12		   = 12,
	VCL_R13		   = 13,
	VCL_N14		   = 14,
	VCL_R15		   = 15,
	BLA_W_LP	   = 16,
	BLA_W_RADL	   = 17,
	BLA_N_LP	   = 18,
	IDR_W_RADL	   = 19,
	IDR_N_LP	   = 20,
	CRA_NUT		   = 21,
	RSV_IRAP_VCL22 = 22,
	RSV_IRAP_VCL23 = 23,
	RSV_VCL24	   = 24,
	RSV_VCL25	   = 25,
	RSV_VCL26	   = 26,
	RSV_VCL27	   = 27,
	RSV_VCL28	   = 28,
	RSV_VCL29	   = 29,
	RSV_VCL30	   = 30,
	RSV_VCL31	   = 31,
	VPS			   = 32,
	SPS			   = 33,
	PPS			   = 34,
	AUD			   = 35,
	EOS			   = 36,
	EOB			   = 37,
	FD			   = 38,
	SEI_PREFIX	   = 39,
	SEI_SUFFIX	   = 40,
	RSV_NVCL41	   = 41,
	RSV_NVCL42	   = 42,
	RSV_NVCL43	   = 43,
	RSV_NVCL44	   = 44,
	RSV_NVCL45	   = 45,
	RSV_NVCL46	   = 46,
	RSV_NVCL47	   = 47,
	UNSPEC48_AP	   = 48,
	UNSPEC49_FU	   = 49,
	UNSPEC50_PACI  = 50,
	UNSPEC51	   = 51,
	UNSPEC52	   = 52,
	UNSPEC53	   = 53,
	UNSPEC54	   = 54,
	UNSPEC55	   = 55,
	UNSPEC56	   = 56,
	UNSPEC57	   = 57,
	UNSPEC58	   = 58,
	UNSPEC59	   = 59,
	UNSPEC60	   = 60,
	UNSPEC61	   = 61,
	UNSPEC62	   = 62,
	UNSPEC63	   = 63,
};

enum HEVCParams{
	// 7.4.3.1:	vps_max_layers_minus1 is in	[0,	62].
	MAX_LAYERS	   = 63,
	// 7.4.3.1:	vps_max_sub_layers_minus1 is in	[0,	6].
	MAX_SUB_LAYERS = 7,
	// 7.4.3.1:	vps_num_layer_sets_minus1 is in	[0,	1023].
	MAX_LAYER_SETS = 1024,

	// 7.4.2.1:	vps_video_parameter_set_id is u(4).
	MAX_VPS_COUNT =	16,
	// 7.4.3.2.1: sps_seq_parameter_set_id is in [0, 15].
	MAX_SPS_COUNT =	16,
	// 7.4.3.3.1: pps_pic_parameter_set_id is in [0, 63].
	MAX_PPS_COUNT =	64,

	// A.4.2: MaxDpbSize is	bounded	above by 16.
	MAX_DPB_SIZE = 16,
	// 7.4.3.1:	vps_max_dec_pic_buffering_minus1[i]	is in [0, MaxDpbSize - 1].
	MAX_REFS	 = MAX_DPB_SIZE,

	// 7.4.3.2.1: num_short_term_ref_pic_sets is in	[0,	64].
	MAX_SHORT_TERM_REF_PIC_SETS	= 64,
	// 7.4.3.2.1: num_long_term_ref_pics_sps is	in [0, 32].
	MAX_LONG_TERM_REF_PICS		= 32,

	// A.3:	all	profiles require that CtbLog2SizeY is in [4, 6].
	MIN_LOG2_CTB_SIZE =	4,
	MAX_LOG2_CTB_SIZE =	6,

	// E.3.2: cpb_cnt_minus1[i]	is in [0, 31].
	MAX_CPB_CNT	= 32,

	// A.4.1: in table A.6 the highest level allows	a MaxLumaPs	of 35 651 584.
	MAX_LUMA_PS	= 35651584,
	// A.4.1: pic_width_in_luma_samples	and	pic_height_in_luma_samples are
	// constrained to be not greater than sqrt(MaxLumaPs * 8).	Hence height/
	// width are bounded above by sqrt(8 * 35651584) = 16888.2 samples.
	MAX_WIDTH  = 16888,
	MAX_HEIGHT = 16888,

	// A.4.1: table	A.6	allows at most 22 tile rows	for	any	level.
	MAX_TILE_ROWS	 = 22,
	// A.4.1: table	A.6	allows at most 20 tile columns for any level.
	MAX_TILE_COLUMNS = 20,

	// A.4.2: table	A.6	allows at most 600 slice segments for any level.
	MAX_SLICE_SEGMENTS = 600,

	// 7.4.7.1:	in the worst case (tiles_enabled_flag and
	// entropy_coding_sync_enabled_flag	are	both set), entry points	can	be
	// placed at the beginning of every	Ctb	row	in every tile, giving an
	// upper bound of (num_tile_columns_minus1 + 1)	* PicHeightInCtbsY - 1.
	// Only	a stream with very high	resolution and perverse	parameters could
	// get near	that, though, so set a lower limit here	with the maximum
	// possible	value for 4K video (at most	135	16x16 Ctb rows).
	MAX_ENTRY_POINT_OFFSETS	= MAX_TILE_COLUMNS * 135,

	// A.3.7: Screen content coding	extensions
	MAX_PALETTE_PREDICTOR_SIZE = 128,
};

const std::array<BYTE, 4> hevc_sub_width_c {
    1, 2, 2, 1
};

const std::array<BYTE, 4> hevc_sub_height_c{
    1, 2, 1, 1
};

struct HEVCWindow {
    DWORD left_offset = 0;
    DWORD right_offset = 0;
    DWORD top_offset = 0;
    DWORD bottom_offset = 0;
};

class H265ProfileTierLevel
{
public:
	bool Decode(BitReader& r);

	BYTE profile_space = 0;
	bool tier_flag = 0;
	BYTE profile_idc = 0;
	std::array<bool, 32> profile_compatibility_flag;
	bool progressive_source_flag	;
	bool interlaced_source_flag		;
	bool non_packed_constraint_flag	;
	bool frame_only_constraint_flag	;

	bool max_12bit_constraint_flag		;
	bool max_10bit_constraint_flag		;
	bool max_8bit_constraint_flag		;
	bool max_422chroma_constraint_flag	;
	bool max_420chroma_constraint_flag	; 
	bool max_monochrome_constraint_flag	;
	bool intra_constraint_flag			;
	bool one_picture_only_constraint_flag	;
	bool lower_bit_rate_constraint_flag		;
	bool max_14bit_constraint_flag			;
	bool inbld_flag							;
	/* 30 times Leverl in Table A.8 – General tier and level limits */
	BYTE level_idc							;
};


class H265SeqParameterSet
{
public:
	H265SeqParameterSet();
	bool Decode(const BYTE*	buffer,DWORD bufferSize, BYTE nuh_layer_id);

private:

	inline static DWORD	Escape(	BYTE *dst,const	BYTE *src, DWORD size )
	{
		DWORD len =	0;
		DWORD i	= 0;
		while(i<size)
		{
			//Check	if next	BYTEs are the scape	sequence
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

	bool ParseProfileTierLevel(BitReader& r, bool profilePresentFlag, BYTE maxNumSubLayersMinus1);

public:
	DWORD GetWidth()	{ return pic_width_in_luma_samples - pic_conf_win.left_offset - pic_conf_win.right_offset; }
	DWORD GetHeight()	{ return pic_height_in_luma_samples - pic_conf_win.top_offset - pic_conf_win.bottom_offset; }

	void Dump()	const
	{
		Debug("[H265SeqParameterSet	\n");
		Debug("\tvps_id=%.2x\n",					vps_id);
		Debug("\tprofile_idc=%.2x\n",				profile_idc);
		Debug("\tconstraint_set0_flag=%u\n",			constraint_set0_flag);
		Debug("\tconstraint_set1_flag=%u\n",			constraint_set1_flag);
		Debug("\tconstraint_set2_flag=%u\n",			constraint_set2_flag);
		Debug("\treserved_zero_5bits =%u\n",			reserved_zero_5bits	);
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
	BYTE			vps_id = 0;
	BYTE			max_sub_layers_minus1 =	0;
	BYTE			ext_or_max_sub_layers_minus1 = 0;
	BYTE			temporal_id_nesting_flag = 0;
	H265ProfileTierLevel generalProfileTierLevel;
	std::array<bool, HEVCParams::MAX_SUB_LAYERS> sub_layer_profile_present_flag;
	std::array<bool, HEVCParams::MAX_SUB_LAYERS> sub_layer_level_present_flag;
	std::array<H265ProfileTierLevel, HEVCParams::MAX_SUB_LAYERS> subLayerProfileTierLevel;
	DWORD			pic_width_in_luma_samples = 0;
	DWORD			pic_height_in_luma_samples = 0;
	bool 			conformance_window_flag = false;
	HEVCWindow		pic_conf_win;

	BYTE			seq_parameter_set_id = 0;
	BYTE			chroma_format_idc = 0;
	bool			separate_colour_plane_flag = false;
	//@Zita	TODO: h264 params, need	double check and updated to	h265
	BYTE			profile_idc	= 0;
	bool			constraint_set0_flag = false;
	bool			constraint_set1_flag = false;
	bool			constraint_set2_flag = false;
	BYTE			reserved_zero_5bits	 = 0;
	BYTE			level_idc =	0;
	//moved to h265 BYTE			seq_parameter_set_id = 0;
	BYTE			log2_max_frame_num_minus4 =	0;
	BYTE			pic_order_cnt_type = 0;
	BYTE			log2_max_pic_order_cnt_lsb_minus4 =	0;
	bool			delta_pic_order_always_zero_flag = false;
	int			offset_for_non_ref_pic = 0;
	int			offset_for_top_to_bottom_field = 0;
	BYTE			num_ref_frames_in_pic_order_cnt_cycle =	0;
	std::vector<int>	offset_for_ref_frame;
	DWORD			num_ref_frames = 0;
	bool			gaps_in_frame_num_value_allowed_flag = false;
	DWORD			pic_width_in_mbs_minus1	= 0;
	DWORD			pic_height_in_map_units_minus1 = 0;
	bool			frame_mbs_only_flag	= false;
	bool			mb_adaptive_frame_field_flag = false;
	bool			direct_8x8_inference_flag =	false;
	bool			frame_cropping_flag	= false;
	DWORD			frame_crop_left_offset = 0;
	DWORD			frame_crop_right_offset	= 0;
	DWORD			frame_crop_top_offset =	0;
	DWORD			frame_crop_bottom_offset = 0;
	//bool			vui_parameters_present_flag	= false;

};

class H265PictureParameterSet
{
public:
	bool Decode(const BYTE*	buffer,DWORD bufferSize)
	{
		//SHould be	done otherway, like	modifying the BitReader	to escape the input	NAL, but anyway.. duplicate	memory
		BYTE *aux =	(BYTE*)malloc(bufferSize);
		//Escape
		DWORD len =	Escape(aux,buffer,bufferSize);
		//Create bit reader
		BitReader r(aux,len);
		//Read SQS
		CHECK(r); pic_parameter_set_id = ExpGolombDecoder::Decode(r);
		CHECK(r); seq_parameter_set_id = ExpGolombDecoder::Decode(r);
		CHECK(r); entropy_coding_mode_flag = r.Get(1);
		CHECK(r); pic_order_present_flag = r.Get(1);
		CHECK(r); num_slice_groups_minus1 =	ExpGolombDecoder::Decode(r);
		if(	num_slice_groups_minus1	> 0	)
		{
			CHECK(r); slice_group_map_type = ExpGolombDecoder::Decode(r);
			if(	slice_group_map_type ==	0 )
				for( int iGroup	= 0; iGroup	<= num_slice_groups_minus1;	iGroup++ )
				{
					CHECK(r); run_length_minus1.assign(iGroup,ExpGolombDecoder::Decode(r));
				}
			else if( slice_group_map_type == 2 )
				for( int iGroup	= 0; iGroup	< num_slice_groups_minus1; iGroup++	)
				{
					CHECK(r); top_left.assign(iGroup,ExpGolombDecoder::Decode(r));
					CHECK(r); bottom_right.assign(iGroup,ExpGolombDecoder::Decode(r));
				}
			else if( slice_group_map_type == 3 || slice_group_map_type ==  4 ||	slice_group_map_type ==	5 )
			{
				CHECK(r); slice_group_change_direction_flag	= r.Get(1);
				CHECK(r); slice_group_change_rate_minus1 = ExpGolombDecoder::Decode(r);
			} else if( slice_group_map_type	== 6 ) {
				CHECK(r); pic_size_in_map_units_minus1 = ExpGolombDecoder::Decode(r);
				for( int i = 0;	i <= pic_size_in_map_units_minus1; i++ )
				{
					CHECK(r); slice_group_id.assign(i,r.Get(ceil(log2(num_slice_groups_minus1+1))));
				}
			}
		}
		CHECK(r); num_ref_idx_l0_active_minus1 = ExpGolombDecoder::Decode(r);
		CHECK(r); num_ref_idx_l1_active_minus1 = ExpGolombDecoder::Decode(r);
		CHECK(r); weighted_pred_flag = r.Get(1);
		CHECK(r); weighted_bipred_idc =	r.Get(2);
		CHECK(r); pic_init_qp_minus26 =	ExpGolombDecoder::DecodeSE(r); //Signed
		CHECK(r); pic_init_qs_minus26 =	ExpGolombDecoder::DecodeSE(r); //Signed
		CHECK(r); chroma_qp_index_offset = ExpGolombDecoder::DecodeSE(r); //Signed
		CHECK(r); deblocking_filter_control_present_flag = r.Get(1);
		CHECK(r); constrained_intra_pred_flag =	r.Get(1);
		CHECK(r); redundant_pic_cnt_present_flag = r.Get(1);

		//Free memory
		free(aux);
		//OK
		return true;
	}
private:
	inline static DWORD	Escape(	BYTE *dst,const	BYTE *src, DWORD size )
	{
		DWORD len =	0;
		DWORD i	= 0;
		while(i<size)
		{
			//Check	if next	BYTEs are the scape	sequence
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
	void Dump()	const
	{
		Debug("[H265PictureParameterSet	\n");
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
	int			num_slice_groups_minus1	= 0;
	BYTE			slice_group_map_type = 0;
	std::vector<DWORD>	run_length_minus1;
	std::vector<DWORD>	top_left;
	std::vector<DWORD>	bottom_right;
	bool			slice_group_change_direction_flag =	false;
	int			slice_group_change_rate_minus1 = 0;
	int			pic_size_in_map_units_minus1 = 0;
	std::vector<DWORD>	slice_group_id;
	BYTE			num_ref_idx_l0_active_minus1 = 0;
	BYTE			num_ref_idx_l1_active_minus1 = 0;
	bool			weighted_pred_flag = false;
	BYTE			weighted_bipred_idc	= 0;
	int			pic_init_qp_minus26	= 0;
	int			pic_init_qs_minus26	= 0;
	int			chroma_qp_index_offset = 0;
	bool			deblocking_filter_control_present_flag = false;
	bool			constrained_intra_pred_flag	= false;
	bool			redundant_pic_cnt_present_flag = false;
};

#undef CHECK

inline void	H265ToAnnexB(BYTE* data, DWORD size)
{
	DWORD pos =	0;

	while (pos < size)
	{
		//Get nal size
		DWORD nalSize =	get4(data, pos);
		//If it	was	already	in annex B
		if (nalSize==1 && !pos)
			//Done
			return;
		//Set annexB start code
		set4(data, pos,	1);
		//Next
		pos	+= 4 + nalSize;
	}
}

#endif	/* H265_H */

