#include "h265.h"

// H.265 uses same stream format (Annex B)
#include "h264/H26xNal.h"

#define CHECK(r) {if(r.Error()) return false;}

bool H265DecodeNalHeader(const BYTE* payload, DWORD payloadLen, BYTE& nalUnitType, BYTE& nuh_layer_id, BYTE& nuh_temporal_id_plus1)
{
	//Check length
	if (payloadLen<HEVCParams::RTP_NAL_HEADER_SIZE)
		//Exit
		return false;

	/* 
	*   +-------------+-----------------+
	*   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
	*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*   |F|   Type    |  LayerId  | TID |
	*   +-------------+-----------------+
	*
	* F must be 0.
	*/

	nalUnitType = (payload[0] & 0x7e) >> 1;
	nuh_layer_id = ((payload[0] & 0x1) << 5) + ((payload[1] & 0xf8) >> 3);
	nuh_temporal_id_plus1 = payload[1] & 0x7;
	UltraDebug("-H265DecodeNalHeader: [NAL header:0x%02x%02x,Nalu type:%d,layer_id:%d, temporal_id:%d]\n", payload[0], payload[1], nalUnitType, nuh_layer_id, nuh_temporal_id_plus1);
	return true;
}

bool H265IsIntra(BYTE nalUnitType)
{
	return ((nalUnitType >= HEVC_RTP_NALU_Type::BLA_W_LP && nalUnitType <= HEVC_RTP_NALU_Type::CRA_NUT) 
			|| (nalUnitType == HEVC_RTP_NALU_Type::VPS)
			|| (nalUnitType == HEVC_RTP_NALU_Type::SPS)
			|| (nalUnitType == HEVC_RTP_NALU_Type::PPS));
}

bool GenericProfileTierLevel::Decode(BitReader& r)
{
	if (r.Left() < 2+1+5 + 32 +	4 +	43 + 1)
	{
		return false;
	}

	CHECK(r); profile_space	= r.Get(2);
	CHECK(r); tier_flag	= r.Get(1);
	CHECK(r); profile_idc =	r.Get(5);

	for	(DWORD i = 0;	i <	HEVCParams::PROFILE_COMPATIBILITY_FLAGS_COUNT /*32*/;	i++)
	{
		CHECK(r); profile_compatibility_flag[i]	= r.Get(1);

		if (profile_idc	==	0 && i > 0 && profile_compatibility_flag[i])
			profile_idc	= i;
	}

	constraing_indicator_flags	= 0;
	CHECK(r); progressive_source_flag		= r.Get(1);
	constraing_indicator_flags |= progressive_source_flag;
	CHECK(r); interlaced_source_flag		= r.Get(1);
	constraing_indicator_flags |= (interlaced_source_flag << 1);
	CHECK(r); non_packed_constraint_flag	= r.Get(1);
	constraing_indicator_flags |= (non_packed_constraint_flag << 2);
	CHECK(r); frame_only_constraint_flag	= r.Get(1);
	constraing_indicator_flags |= (frame_only_constraint_flag << 3);

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

		constraing_indicator_flags |= (max_12bit_constraint_flag << 4);		  
		constraing_indicator_flags |= (max_10bit_constraint_flag << 5);		  
		constraing_indicator_flags |= (max_8bit_constraint_flag << 6);		  
		constraing_indicator_flags |= (max_422chroma_constraint_flag << 7);		  
		constraing_indicator_flags |= (max_420chroma_constraint_flag << 8);		  
		constraing_indicator_flags |= (max_monochrome_constraint_flag << 9);		  
		constraing_indicator_flags |= (intra_constraint_flag << 10);		  
		constraing_indicator_flags |= (one_picture_only_constraint_flag << 11);		  
		constraing_indicator_flags |= (lower_bit_rate_constraint_flag << 12);		  

		if (CheckProfileIdc(5) ||	CheckProfileIdc(9) ||	CheckProfileIdc(10))
		{
			CHECK(r); max_14bit_constraint_flag	  =	r.Get(1);
			constraing_indicator_flags |= (max_14bit_constraint_flag << 13);		  
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
		constraing_indicator_flags |= (one_picture_only_constraint_flag << 11);		  
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
		constraing_indicator_flags |= ((QWORD)(inbld_flag) << 47);		  
	}
	else
	{
		r.Skip(1);
	}

	return true;
}

H265ProfileTierLevel::H265ProfileTierLevel()
{
	for (size_t i = 0; i < sub_layer_profile_present_flag.size(); i++)
		sub_layer_profile_present_flag[i] = false;
	for (size_t i = 0; i < sub_layer_level_present_flag.size(); i++)
		sub_layer_level_present_flag[i] = false;
}

bool H265ProfileTierLevel::Decode(BitReader& r, bool profilePresentFlag, BYTE maxNumSubLayersMinus1)
{
	if (profilePresentFlag)
	{
		if(!general_profile_tier_level.Decode(r) ||
			r.Left() < 8 + (8*2	* (maxNumSubLayersMinus1 > 0)))
		{
			Error("-H265: PTL information too short\n");
			return false;
		}
	}

	CHECK(r); general_profile_tier_level.SetLevelIdc(r.Get(8));
	Debug("-H265: [general_profile_level_idc: %d, level: %.02f]\n", general_profile_tier_level.GetLevelIdc(), static_cast<float>(general_profile_tier_level.GetLevelIdc()) / 30);

	for (int i = 0; i	< maxNumSubLayersMinus1; i++) {
		CHECK(r); sub_layer_profile_present_flag[i] = r.Get(1);
		CHECK(r); sub_layer_level_present_flag[i]	 = r.Get(1);
	}
	if (maxNumSubLayersMinus1 > 0)
		for (int i = maxNumSubLayersMinus1; i < 8; i++)
			r.Skip(2);	// reserved_zero_2bits[i]

	for (size_t i = 0; i < maxNumSubLayersMinus1; i++)
	{
		if (sub_layer_profile_present_flag[i])
		{ 
			if(!sub_layer_profile_tier_level[i].Decode(r))
			{
				Error("PTL information for sublayer %zu too short\n",	i);
				return false;
			}
		}
		if (sub_layer_level_present_flag[i])
		{
			if (r.Left() < 8)
			{
				Error("Not enough data for sublayer %zu level_idc\n",	i);
				return false;
			}
			else
			{
				CHECK(r); sub_layer_profile_tier_level[i].SetLevelIdc(r.Get(8));
				Debug("-H265: [sub_layer[%zu].level_idc: %d, level: %.02f]\n", i, sub_layer_profile_tier_level[i].GetLevelIdc(), static_cast<float>(sub_layer_profile_tier_level[i].GetLevelIdc()) / 30);
			}
		}
	}

	return true;
}

H265VideoParameterSet::H265VideoParameterSet()
{}

bool H265VideoParameterSet::Decode(const BYTE* buffer,DWORD bufferSize)
{
	//SHould be	done otherway, like	modifying the BitReader	to escape the input	NAL, but anyway.. duplicate	memory
	BYTE *aux =	(BYTE*)malloc(bufferSize);
	//Escape
	DWORD len =	NalUnescapeRbsp(aux,buffer,bufferSize);

	//Create bit reader
	BitReader r(aux,len);

	CHECK(r); vps_id = r.Get(4);
	r.Skip(1); // vps_base_layer_internal_flag
	r.Skip(1); // vps_base_layer_available_flag

	CHECK(r); vps_max_layers_minus1				  = r.Get(6);
	CHECK(r); vps_max_sub_layers_minus1			  = r.Get(3);
	CHECK(r); vps_temporal_id_nesting_flag = r.Get(1);

	CHECK(r); WORD reserved = r.Get(16); 
	if (reserved != 0xffff) { // vps_reserved_ffff_16bits
		Error("vps_reserved_ffff_16bits is not 0xffff\n");
		return false;
	}

	if (vps_max_sub_layers_minus1 + 1 > HEVCParams::MAX_SUB_LAYERS) {
		Error("vps_max_sub_layers_minus1 out of range: %d\n", vps_max_sub_layers_minus1);
		return false;
	}

	if (!profile_tier_level.Decode(r, true, vps_max_sub_layers_minus1))
	{
		return false;
	}
	// skip all the following element decode/parse
	return true;
}

bool H265SeqParameterSet::Decode(const BYTE* buffer,DWORD bufferSize)
{
	BYTE *aux =	(BYTE*)malloc(bufferSize);
	//Escape
	DWORD len =	NalUnescapeRbsp(aux,buffer,bufferSize);
	//Create bit reader
	BitReader r(aux,len);
	//Read SPS
	CHECK(r); vps_id = r.Get(4);

	const BYTE nuh_layer_id = 0; // sub layers are not supported at the moment
	// profiel_level_tier 
	if (nuh_layer_id ==	0)
	{
		CHECK(r); max_sub_layers_minus1	= r.Get(3);
	}
	else
	{
		CHECK(r); ext_or_max_sub_layers_minus1 = r.Get(3);
	}
	if (max_sub_layers_minus1 >	HEVCParams::MAX_SUB_LAYERS - 1)	{
		Error( "sps_max_sub_layers_minus1 out of range: %d\n", max_sub_layers_minus1);
		return false;
	}
	const bool MultiLayerExtSpsFlag =	(nuh_layer_id != 0)	&& (ext_or_max_sub_layers_minus1 ==	7);
	if (!MultiLayerExtSpsFlag)
	{
		CHECK(r); temporal_id_nesting_flag = r.Get(1);
		if (!profile_tier_level.Decode(r, true,	max_sub_layers_minus1))
			return false;
	}
	// sps id
	seq_parameter_set_id =	ExpGolombDecoder::Decode(r);
	if (seq_parameter_set_id >= HEVCParams::MAX_SPS_COUNT) {
		Error("SPS id out of range:	%d\n", seq_parameter_set_id);
		return false;
	}
	// chromo_format_idc
	chroma_format_idc = ExpGolombDecoder::Decode(r);
	if (chroma_format_idc > 3U) {
		Error("chroma_format_idc %d	is invalid\n", chroma_format_idc);
		return false;
	}
	if (chroma_format_idc == 3)
	{
		CHECK(r); separate_colour_plane_flag = r.Get(1);
	}
	if(separate_colour_plane_flag)
		chroma_format_idc = 0;
	// width & height in luma
	pic_width_in_luma_samples  = ExpGolombDecoder::Decode(r);
	pic_height_in_luma_samples = ExpGolombDecoder::Decode(r);
	// conformance window
	conformance_window_flag = r.Get(1);
	if (conformance_window_flag) {
		BYTE vert_mult	= hevc_sub_height_c[chroma_format_idc];
		BYTE horiz_mult = hevc_sub_width_c[chroma_format_idc];
		pic_conf_win.left_offset   = ExpGolombDecoder::Decode(r) * horiz_mult;
		pic_conf_win.right_offset  = ExpGolombDecoder::Decode(r) * horiz_mult;
		pic_conf_win.top_offset    = ExpGolombDecoder::Decode(r) *	vert_mult;
		pic_conf_win.bottom_offset = ExpGolombDecoder::Decode(r) *	vert_mult;
	}

	CHECK(r); bit_depth_luma_minus8 = ExpGolombDecoder::Decode(r); 
	CHECK(r); bit_depth_chroma_minus8 = ExpGolombDecoder::Decode(r);
	CHECK(r); log2_max_pic_order_cnt_lsb_minus4 = ExpGolombDecoder::Decode(r);
	CHECK(r); sps_sub_layer_ordering_info_present_flag = r.Get(1);
	
	for (size_t i = (sps_sub_layer_ordering_info_present_flag ? 0 : max_sub_layers_minus1);
	     i <= max_sub_layers_minus1; i++)
	{
		CHECK(r); ExpGolombDecoder::Decode(r);  // sps_max_dec_pic_buffering_minus1[ i ]
		CHECK(r); ExpGolombDecoder::Decode(r);  // sps_max_num_reorder_pics[ i ]
		CHECK(r); ExpGolombDecoder::Decode(r);  // sps_max_latency_increase_plus1[ i ]
	}
	
	CHECK(r); log2_min_luma_coding_block_size_minus3 = ExpGolombDecoder::Decode(r);
	CHECK(r); log2_diff_max_min_luma_coding_block_size = ExpGolombDecoder::Decode(r);
	CHECK(r); log2_min_luma_transform_block_size_minus2 = ExpGolombDecoder::Decode(r);
	CHECK(r); log2_diff_max_min_luma_transform_block_size = ExpGolombDecoder::Decode(r);

	// skip all following SPS element
	//Free memory
	free(aux);
	
	// Calculate log2PicSizeInCtbsY
	auto MinCbLog2SizeY = log2_min_luma_coding_block_size_minus3 + 3;
	auto CtbLog2SizeY = MinCbLog2SizeY + log2_diff_max_min_luma_coding_block_size;
	auto CtbSizeY = 1 << CtbLog2SizeY;	
	auto PicWidthInCtbsY = ceil(pic_width_in_luma_samples / double(CtbSizeY));
	log2PicSizeInCtbsY = std::ceil(log2(PicWidthInCtbsY));
	
	//OK
	return !r.Error();
}

bool H265PictureParameterSet::Decode(const BYTE* buffer,DWORD bufferSize)
{
	//SHould be	done otherway, like	modifying the BitReader	to escape the input	NAL, but anyway.. duplicate	memory
	BYTE *aux =	(BYTE*)malloc(bufferSize);
	//Escape
	DWORD len =	NalUnescapeRbsp(aux,buffer,bufferSize);
	//Create bit reader
	BitReader r(aux,len);

	pps_id = ExpGolombDecoder::Decode(r);
	if (pps_id >= HEVCParams::MAX_PPS_COUNT) {
		Error("PPS id out of range: %d !\n", pps_id);
		return false;
	}
	sps_id = ExpGolombDecoder::Decode(r);
	if (sps_id >= HEVCParams::MAX_SPS_COUNT) {
		Error("SPS id out of range: %d !\n", sps_id);
		return false;
	}

	CHECK(r); dependent_slice_segments_enabled_flag = r.Get(1);
	CHECK(r); output_flag_present_flag				= r.Get(1);
	CHECK(r); num_extra_slice_header_bits			= r.Get(3);

	CHECK(r); sign_data_hiding_flag = r.Get(1);
	CHECK(r); cabac_init_present_flag = r.Get(1);

	CHECK(r); num_ref_idx_l0_default_active_minus1 = ExpGolombDecoder::Decode(r);
	CHECK(r); num_ref_idx_l1_default_active_minus1 = ExpGolombDecoder::Decode(r);
	if (num_ref_idx_l0_default_active_minus1 + 1 >= HEVCParams::MAX_REFS ||
		num_ref_idx_l1_default_active_minus1 + 1 >= HEVCParams::MAX_REFS) {
		Error("Too many default refs in PPS: %d/%d.\n",
			   num_ref_idx_l0_default_active_minus1 + 1, num_ref_idx_l1_default_active_minus1 + 1);
		return false;
	}

	//Free memory
	free(aux);
	//OK
	return true;
}

