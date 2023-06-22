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

struct HEVCParams{
	// 7.4.3.1:	vps_max_layers_minus1 is in	[0,	62].
	static const BYTE  MAX_LAYERS	   = 63;
	// 7.4.3.1:	vps_max_sub_layers_minus1 is in	[0,	6].
	static const BYTE MAX_SUB_LAYERS = 7;
	// 7.4.3.1:	vps_num_layer_sets_minus1 is in	[0,	1023].
	static const WORD MAX_LAYER_SETS = 1024;

	// 7.4.2.1:	vps_video_parameter_set_id is u(4).
	static const BYTE MAX_VPS_COUNT =	16;
	// 7.4.3.2.1: sps_seq_parameter_set_id is in [0, 15].
	static const BYTE MAX_SPS_COUNT =	16;
	// 7.4.3.3.1: pps_pic_parameter_set_id is in [0, 63].
	static const BYTE MAX_PPS_COUNT =	64;

	// A.4.2: MaxDpbSize is	bounded	above by 16.
	static const BYTE MAX_DPB_SIZE = 16;
	// 7.4.3.1:	vps_max_dec_pic_buffering_minus1[i]	is in [0, MaxDpbSize - 1].
	static const BYTE MAX_REFS	 = MAX_DPB_SIZE;

	// 7.4.3.2.1: num_short_term_ref_pic_sets is in	[0,	64].
	static const BYTE MAX_SHORT_TERM_REF_PIC_SETS	= 64;
	// 7.4.3.2.1: num_long_term_ref_pics_sps is	in [0, 32].
	static const BYTE MAX_LONG_TERM_REF_PICS		= 32;

	// A.3:	all	profiles require that CtbLog2SizeY is in [4, 6].
	static const BYTE MIN_LOG2_CTB_SIZE =	4;
	static const BYTE MAX_LOG2_CTB_SIZE =	6;

	// E.3.2: cpb_cnt_minus1[i]	is in [0, 31].
	static const BYTE MAX_CPB_CNT	= 32;

	// A.4.1: in table A.6 the highest level allows	a MaxLumaPs	of 35 651 584.
	static const DWORD MAX_LUMA_PS	= 35651584;
	// A.4.1: pic_width_in_luma_samples	and	pic_height_in_luma_samples are
	// constrained to be not greater than sqrt(MaxLumaPs * 8).	Hence height/
	// width are bounded above by sqrt(8 * 35651584) = 16888.2 samples.
	static const WORD MAX_WIDTH  = 16888;
	static const WORD MAX_HEIGHT = 16888;

	// A.4.1: table	A.6	allows at most 22 tile rows	for	any	level.
	static const BYTE MAX_TILE_ROWS	 = 22;
	// A.4.1: table	A.6	allows at most 20 tile columns for any level.
	static const BYTE MAX_TILE_COLUMNS = 20;

	// A.4.2: table	A.6	allows at most 600 slice segments for any level.
	static const WORD MAX_SLICE_SEGMENTS = 600;

	// 7.4.7.1:	in the worst case (tiles_enabled_flag and
	// entropy_coding_sync_enabled_flag	are	both set), entry points	can	be
	// placed at the beginning of every	Ctb	row	in every tile, giving an
	// upper bound of (num_tile_columns_minus1 + 1)	* PicHeightInCtbsY - 1.
	// Only	a stream with very high	resolution and perverse	parameters could
	// get near	that, though, so set a lower limit here	with the maximum
	// possible	value for 4K video (at most	135	16x16 Ctb rows).
	static const WORD MAX_ENTRY_POINT_OFFSETS	= MAX_TILE_COLUMNS * 135;

	// A.3.7: Screen content coding	extensions
	static const BYTE MAX_PALETTE_PREDICTOR_SIZE = 128;

	static const BYTE PROFILE_COMPATIBILITY_FLAGS_COUNT = 32;

	static const BYTE ANNEX_B_START_CODE = 0x01;

	// HEVC PROFILE
	static const BYTE PROFILE_MAIN                        = 1;
	static const BYTE PROFILE_MAIN_10                     = 2;
	static const BYTE PROFILE_MAIN_STILL_PICTURE          = 3;
	static const BYTE PROFILE_REXT                        = 4;
	static const BYTE PROFILE_SCC                         = 9;

	// RFC 7998
	static const BYTE RTP_NAL_HEADER_SIZE				= 2;
	/* 
     *   +-------------+-----------------+
     *   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |F|   Type    |  LayerId  | TID |
     *   +-------------+-----------------+
	 *
	 * F must be 0.
	 */
};

const std::array<BYTE, 4> hevc_sub_width_c {
	1, 2, 2, 1
};

const std::array<BYTE, 4> hevc_sub_height_c{
	1, 2, 1, 1
};

struct HEVCWindow {
	DWORD left_offset =	0;
	DWORD right_offset = 0;
	DWORD top_offset = 0;
	DWORD bottom_offset	= 0;

	void Dump(const std::string name) const
	{
		Debug("\t%s.left_offset = %d\n", name.c_str(), left_offset);
		Debug("\t%s.right_offset = %d\n", name.c_str(), right_offset);
		Debug("\t%s.top_offset = %d\n", name.c_str(), top_offset);
		Debug("\t%s.bottom_offset = %d\n", name.c_str(), bottom_offset);
	}
};

DWORD H265Escape(BYTE *dst,const BYTE *src, DWORD size);

typedef std::array<bool, HEVCParams::PROFILE_COMPATIBILITY_FLAGS_COUNT> H265ProfileCompatibilityFlags;

class GenericProfileTierLevel
{
public:
	GenericProfileTierLevel()
	{
		for (size_t i = 0; i < profile_compatibility_flag.size(); i++)
		{
			profile_compatibility_flag[i] = (i == profile_idc)? true:false;
		}
	}

	bool Decode(BitReader& r);
	// getter
	BYTE GetProfileSpace() const { return profile_space; }
	BYTE GetProfileIdc() const { return profile_idc; }
	bool GetTierFlag() const { return tier_flag; }
	BYTE GetLevelIdc() const { return level_idc; }
	const H265ProfileCompatibilityFlags& GetProfileCompatibilityFlags() const { return profile_compatibility_flag; }
	bool GetProgressiveSourceFlag() const { return progressive_source_flag; }
	bool GetInterlacedSourceFlag() const { return interlaced_source_flag; }
	bool GetNonPackedConstraintFlag() const { return non_packed_constraint_flag; }
	bool GetFrameOnlyConstraintFlag() const { return frame_only_constraint_flag; }
	// setter
	void SetLevelIdc(BYTE in) { level_idc = in; }

private:
	// initial values refer to RFC 7798 section 7.1
	BYTE profile_space = 0;
	bool tier_flag = 0;
	BYTE profile_idc = HEVCParams::PROFILE_MAIN; // 1
	H265ProfileCompatibilityFlags profile_compatibility_flag; // [PROFILE_MAIN/1]: true, others are false
	bool progressive_source_flag	= true  ;
	bool interlaced_source_flag		= false ;
	bool non_packed_constraint_flag	= true  ;
	bool frame_only_constraint_flag	= true  ;

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
	/* 30 times	Leverl in Table	A.8	– General tier and level limits	*/
	BYTE level_idc = 93	; // level 3.1
};

class H265ProfileTierLevel
{
public:
	H265ProfileTierLevel();
	bool Decode(BitReader& r, bool profilePresentFlag, BYTE	maxNumSubLayersMinus1);
	BYTE GetGeneralProfileSpace() const { return generalProfileTierLevel.GetProfileSpace(); }
	BYTE GetGeneralProfileIdc() const { return generalProfileTierLevel.GetProfileIdc(); }
	bool GetGeneralTierFlag() const { return generalProfileTierLevel.GetTierFlag(); }
	BYTE GetGeneralLevelIdc() const { return generalProfileTierLevel.GetLevelIdc(); }
	const H265ProfileCompatibilityFlags& GetGeneralProfileCompatibilityFlags() const { return generalProfileTierLevel.GetProfileCompatibilityFlags(); }
	bool GetGeneralProgressiveSourceFlag() const { return generalProfileTierLevel.GetProgressiveSourceFlag(); }
	bool GetGeneralInterlacedSourceFlag() const { return generalProfileTierLevel.GetInterlacedSourceFlag(); }
	bool GetGeneralNonPackedConstraintFlag() const { return generalProfileTierLevel.GetNonPackedConstraintFlag(); }
	bool GetGeneralFrameOnlyConstraintFlag() const { return generalProfileTierLevel.GetFrameOnlyConstraintFlag(); }

	void Dump() const
	{
		Warning("-H265: %s is not finished yet!\n", __PRETTY_FUNCTION__);
	}

private:
	GenericProfileTierLevel	generalProfileTierLevel;
	std::array<bool, HEVCParams::MAX_SUB_LAYERS> sub_layer_profile_present_flag;
	std::array<bool, HEVCParams::MAX_SUB_LAYERS> sub_layer_level_present_flag;
	std::array<GenericProfileTierLevel, HEVCParams::MAX_SUB_LAYERS> subLayerProfileTierLevel;
};

class H265VideoParameterSet
{
public:
	H265VideoParameterSet();
	bool Decode(const BYTE*	buffer, DWORD bufferSize);
	void Dump()	const
	{
		Debug("[H265VideoParameterSet\n");
		Debug("\tvps_id = %d\n", vps_id);
		Debug("\tvps_max_layers_minus1 = %d\n", vps_max_layers_minus1);
		Debug("\tvps_max_sub_layers_minus1 = %d\n", vps_max_sub_layers_minus1);
		Debug("\tvps_temporal_id = %d\n", vps_temporal_id_nesting_flag);
		profileTierLevel.Dump();
		Debug("H265VideoParameterSet/]\n");
	}

	const H265ProfileTierLevel& GetProfileTierLevel() const {return profileTierLevel;}

private:
	BYTE vps_id;
	BYTE vps_max_layers_minus1		;
	BYTE vps_max_sub_layers_minus1	;
	bool vps_temporal_id_nesting_flag  = false;
	H265ProfileTierLevel profileTierLevel;
};

class H265SeqParameterSet
{
public:
	bool Decode(const BYTE*	buffer, DWORD buffersize, BYTE nuh_layer_id);

	DWORD GetWidth()	{ return pic_width_in_luma_samples - pic_conf_win.left_offset -	pic_conf_win.right_offset; }
	DWORD GetHeight()	{ return pic_height_in_luma_samples	- pic_conf_win.top_offset -	pic_conf_win.bottom_offset;	}

	void Dump()	const
	{
		Debug("[H265SeqParameterSet\n");
		Debug("\tvps_id=%.2x\n", vps_id);
		Debug("\tmax_sub_layers_minus1 = %d\n", max_sub_layers_minus1);
		Debug("\text_or_max_sub_layers_minus1 = %d\n", ext_or_max_sub_layers_minus1);
		Debug("\ttemporal_id_nesting_flag = %d\n", temporal_id_nesting_flag);
		profileTierLevel.Dump();
		Debug("\tpic_width_in_luma_samples = %d\n", pic_width_in_luma_samples);
		Debug("\tpic_height_in_luma_samples = %d\n", pic_height_in_luma_samples);
		Debug("\tconformance_window_flag = %d\n", conformance_window_flag );
		pic_conf_win.Dump("pic_conf_win");
		Debug("\tseq_parameter_set_id = %d\n", seq_parameter_set_id);
		Debug("\tchroma_format_idc = %d\n", chroma_format_idc);
		Debug("\tseparate_colour_plane_flag = %d\n", separate_colour_plane_flag);
		Debug("H265SeqParameterSet/]\n");
	}
private:
	BYTE			vps_id = 0;
	BYTE			max_sub_layers_minus1 =	0;
	BYTE			ext_or_max_sub_layers_minus1 = 0;
	BYTE			temporal_id_nesting_flag = 0;
	H265ProfileTierLevel profileTierLevel;
	DWORD			pic_width_in_luma_samples =	0;
	DWORD			pic_height_in_luma_samples = 0;
	bool			conformance_window_flag	= false;
	HEVCWindow		pic_conf_win;

	BYTE			seq_parameter_set_id = 0;
	BYTE			chroma_format_idc =	0;
	bool			separate_colour_plane_flag = false;

};

class H265PictureParameterSet
{
public:
	bool Decode(const BYTE*	buffer,DWORD bufferSize)
	{
		//SHould be	done otherway, like	modifying the BitReader	to escape the input	NAL, but anyway.. duplicate	memory
		BYTE *aux =	(BYTE*)malloc(bufferSize);
		//Escape
		DWORD len =	H265Escape(aux,buffer,bufferSize);
		//Create bit reader
		BitReader r(aux,len);

		Warning("-H265: %s is not finished yet!\n", __PRETTY_FUNCTION__);

		//Free memory
		free(aux);
		//OK
		return true;
	}

	void Dump()	const
	{
		Warning("-H265: %s is not finished yet!\n", __PRETTY_FUNCTION__);
	}

private:
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

