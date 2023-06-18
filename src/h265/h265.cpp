#include "h265.h"

#define CHECK(r) {if(r.Error()) return false;}

bool H265ProfileTierLevel::Decode(BitReader& r)
{
	if (r.Left() < 2+1+5 + 32 +	4 +	43 + 1)
	{
		return false;
	}

	CHECK(r); profile_space	= r.Get(2);
	CHECK(r); tier_flag	= r.Get(1);
	CHECK(r); profile_idc =	r.Get(5);

	for	(int i = 0;	i <	32;	i++) {
		CHECK(r); profile_compatibility_flag[i]	= r.Get(1);

		if (profile_idc	==	0 && i > 0 && profile_compatibility_flag[i])
			profile_idc	= i;
	}

	CHECK(r); progressive_source_flag		= r.Get(1);
	CHECK(r); interlaced_source_flag		= r.Get(1);
	CHECK(r); non_packed_constraint_flag	= r.Get(1);
	CHECK(r); frame_only_constraint_flag	= r.Get(1);

	auto CheckProfileIdc = [&](unsigned	char idc){
		return profile_idc	==	idc	|| profile_compatibility_flag[idc];
	};

	if (CheckProfileIdc(4) ||	CheckProfileIdc(5) ||	CheckProfileIdc(6) ||
		CheckProfileIdc(7) ||	CheckProfileIdc(8) ||	CheckProfileIdc(9) ||
		CheckProfileIdc(10))
	{
		CHECK(r); max_12bit_constraint_flag		 = r.Get(1); 
		CHECK(r); max_10bit_constraint_flag		 = r.Get(1); 
		CHECK(r); max_8bit_constraint_flag		 = r.Get(1);
		CHECK(r); max_422chroma_constraint_flag	 = r.Get(1);
		CHECK(r); max_420chroma_constraint_flag	 = r.Get(1);
		CHECK(r); max_monochrome_constraint_flag	= r.Get(1);
		CHECK(r); intra_constraint_flag				= r.Get(1);
		CHECK(r); one_picture_only_constraint_flag	= r.Get(1);
		CHECK(r); lower_bit_rate_constraint_flag	= r.Get(1);

		if (CheckProfileIdc(5) ||	CheckProfileIdc(9) ||	CheckProfileIdc(10))
		{
			CHECK(r); max_14bit_constraint_flag	  =	r.Get(1);
			r.Skip(33);	// XXX_reserved_zero_33bits[0..32]
		}
		else
		{
			r.Skip(34);	// XXX_reserved_zero_34bits[0..33]
		}
	}
	else if (CheckProfileIdc(2))
	{
		r.Skip(7);
		CHECK(r); one_picture_only_constraint_flag = r.Get(1);
		r.Skip(35);	// XXX_reserved_zero_35bits[0..34]
	}
	else
	{
		r.Skip(43);	// XXX_reserved_zero_35bits[0..42]
	}

	if (CheckProfileIdc(1) ||	CheckProfileIdc(2) ||	CheckProfileIdc(3) ||
		CheckProfileIdc(4) ||	CheckProfileIdc(5) ||	CheckProfileIdc(9))
	{
		CHECK(r); inbld_flag	= r.Get(1);
	}
	else
	{
		r.Skip(1);
	}

	return true;
}

bool H265SeqParameterSet::Decode(const BYTE*	buffer,DWORD bufferSize, BYTE nuh_layer_id)
{
	Debug("\t @Zita	TODO: H265SeqParameterSet::Decode()	is not finished	yet!\n");
	//SHould be	done otherway, like	modifying the BitReader	to escape the input	NAL, but anyway.. duplicate	memory
	BYTE *aux =	(BYTE*)malloc(bufferSize);
	//Escape
	DWORD len =	Escape(aux,buffer,bufferSize);
	//Create bit reader
	BitReader r(aux,len);
	//Read SPS
	CHECK(r); vps_id = r.Get(4);
	// ttxgz: TODO:	should check vps list is correct or	not
	//if (vps_list && !vps_list[sps->vps_id]) {
	//	av_log(avctx, AV_LOG_ERROR,	"VPS %d	does not exist\n",
	//	sps->vps_id);
	//	return AVERROR_INVALIDDATA;
	//}
	if (nuh_layer_id ==	0)
	{
		CHECK(r); max_sub_layers_minus1	= r.Get(3);
	}
	else
	{
		CHECK(r); ext_or_max_sub_layers_minus1 = r.Get(3);
	}

	if (max_sub_layers_minus1 >	HEVCParams::MAX_SUB_LAYERS - 1)	{
		Error( "sps_max_sub_layers_minus1 out of range:	%d\n", max_sub_layers_minus1);
		return false;
	}

	bool MultiLayerExtSpsFlag =	(nuh_layer_id != 0)	&& (ext_or_max_sub_layers_minus1 ==	7);
	if (!MultiLayerExtSpsFlag)
	{
		CHECK(r); temporal_id_nesting_flag = r.Get(1);
		if (!ParseProfileTierLevel(r, true,	max_sub_layers_minus1))
			return false;
	}




//*sps_id =	get_ue_golomb_long(gb);
//if (*sps_id >= HEVC_MAX_SPS_COUNT) {
//	  av_log(avctx,	AV_LOG_ERROR, "SPS id out of range:	%d\n", *sps_id);
//	  return AVERROR_INVALIDDATA;
//}

//sps->chroma_format_idc = get_ue_golomb_long(gb);
//if (sps->chroma_format_idc > 3U) {
//	  av_log(avctx,	AV_LOG_ERROR, "chroma_format_idc %d	is invalid\n", sps->chroma_format_idc);
//	  return AVERROR_INVALIDDATA;
//}

//if (sps->chroma_format_idc ==	3)
//	  sps->separate_colour_plane_flag =	get_bits1(gb);

//if (sps->separate_colour_plane_flag)
//	  sps->chroma_format_idc = 0;

//sps->width  =	get_ue_golomb_long(gb);
//sps->height =	get_ue_golomb_long(gb);

	//Free memory
	free(aux);
	//OK
	return !r.Error();
}

bool H265SeqParameterSet::ParseProfileTierLevel(BitReader& r, bool profilePresentFlag, BYTE maxNumSubLayersMinus1)
{
	Error("\t @Zita	TODO: profile_tier_level parser() is not finished yet!\n");

	if(!generalProfileTierLevel.Decode(r) ||
		r.Left() < 8 + (8*2	* (maxNumSubLayersMinus1 > 0)))
	{
		Error("-H265: PTL information too short\n");
		return false;
	}


	CHECK(r); generalProfileTierLevel.level_idc = r.Get(8);
	Debug("-H265: [general_profile_idc: %d]\n", generalProfileTierLevel.level_idc);

//for (i = 0; i	< max_num_sub_layers - 1; i++) {
//	  ptl->sub_layer_profile_present_flag[i] = get_bits1(gb);
//	  ptl->sub_layer_level_present_flag[i]	 = get_bits1(gb);
//}

//if (max_num_sub_layers - 1> 0)
//	  for (i = max_num_sub_layers -	1; i < 8; i++)
//		  skip_bits(gb,	2);	// reserved_zero_2bits[i]
//for (i = 0; i	< max_num_sub_layers - 1; i++) {
//	  if (ptl->sub_layer_profile_present_flag[i] &&
//		  decode_profile_tier_level(gb,	avctx, &ptl->sub_layer_ptl[i]) < 0)	{
//		  av_log(avctx,	AV_LOG_ERROR,
//				 "PTL information for sublayer %i too short\n",	i);
//		  return -1;
//	  }
//	  if (ptl->sub_layer_level_present_flag[i])	{
//		  if (get_bits_left(gb)	< 8) {
//			  av_log(avctx,	AV_LOG_ERROR,
//					 "Not enough data for sublayer %i level_idc\n",	i);
//			  return -1;
//		  }	else
//			  ptl->sub_layer_ptl[i].level_idc =	get_bits(gb, 8);
//	  }
//}

//return 0;
	return true;
}