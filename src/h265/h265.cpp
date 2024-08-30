#include "h265.h"

// H.265 uses same stream format (Annex B)
#include "h264/H26xNal.h"

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
	try
	{

		profile_space	= r.Get(2);
		tier_flag	= r.Get(1);
		profile_idc	= r.Get(5);

		for (DWORD i = 0; i < HEVCParams::PROFILE_COMPATIBILITY_FLAGS_COUNT /*32*/; i++)
		{
			profile_compatibility_flag[i] = r.Get(1);

			if (profile_idc	== 0 && i > 0 && profile_compatibility_flag[i])
				profile_idc = i;
		}

		constraing_indicator_flags	= 0;
		progressive_source_flag		= r.Get(1);
		constraing_indicator_flags |= progressive_source_flag;
		interlaced_source_flag		= r.Get(1);
		constraing_indicator_flags |= (interlaced_source_flag << 1);
		non_packed_constraint_flag	= r.Get(1);
		constraing_indicator_flags |= (non_packed_constraint_flag << 2);
		frame_only_constraint_flag	= r.Get(1);
		constraing_indicator_flags |= (frame_only_constraint_flag << 3);

		auto CheckProfileIdc = [&](unsigned char idc) {
			return profile_idc == idc || profile_compatibility_flag[idc];
		};

		if (CheckProfileIdc(4) ||	CheckProfileIdc(5) ||	CheckProfileIdc(6) ||
			CheckProfileIdc(7) ||	CheckProfileIdc(8) ||	CheckProfileIdc(9) ||
			CheckProfileIdc(10))
		{
			max_12bit_constraint_flag	 = r.Get(1); 
			max_10bit_constraint_flag	 = r.Get(1); 
			max_8bit_constraint_flag	 = r.Get(1);
			max_422chroma_constraint_flag	 = r.Get(1);
			max_420chroma_constraint_flag	 = r.Get(1);
			max_monochrome_constraint_flag	 = r.Get(1);
			intra_constraint_flag		 = r.Get(1);
			one_picture_only_constraint_flag = r.Get(1);
			lower_bit_rate_constraint_flag	 = r.Get(1);

			constraing_indicator_flags |= (max_12bit_constraint_flag << 4);		  
			constraing_indicator_flags |= (max_10bit_constraint_flag << 5);		  
			constraing_indicator_flags |= (max_8bit_constraint_flag << 6);		  
			constraing_indicator_flags |= (max_422chroma_constraint_flag << 7);		  
			constraing_indicator_flags |= (max_420chroma_constraint_flag << 8);		  
			constraing_indicator_flags |= (max_monochrome_constraint_flag << 9);		  
			constraing_indicator_flags |= (intra_constraint_flag << 10);		  
			constraing_indicator_flags |= (one_picture_only_constraint_flag << 11);		  
			constraing_indicator_flags |= (lower_bit_rate_constraint_flag << 12);		  

			if (CheckProfileIdc(5) || CheckProfileIdc(9) ||	CheckProfileIdc(10))
			{
				max_14bit_constraint_flag   = r.Get(1);
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
			one_picture_only_constraint_flag = r.Get(1);
			constraing_indicator_flags |= (one_picture_only_constraint_flag << 11);		  
			r.Skip(35);	// XXX_reserved_zero_35bits[0..34]
		}
		else
		{
			r.Skip(43);	// XXX_reserved_zero_35bits[0..42]
		}

		if (CheckProfileIdc(1) || CheckProfileIdc(2) ||	CheckProfileIdc(3) || 
		    CheckProfileIdc(4) || CheckProfileIdc(5) ||	CheckProfileIdc(9))
		{
			inbld_flag	= r.Get(1);
			constraing_indicator_flags |= ((QWORD)(inbld_flag) << 47);		  
		}
		else
		{
			r.Skip(1);
		}
	} 
	catch (std::exception& e)
	{
		return false;
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
		if(!general_profile_tier_level.Decode(r))
			return Warning("-H265ProfileTierLevel::Decode() Wrong PTL\n");
		
	}

	general_profile_tier_level.SetLevelIdc(r.Get(8));

	//UltraDebug("-H265ProfileTierLevel::Decode() [general_profile_level_idc: %d, level: %.02f]\n", general_profile_tier_level.GetLevelIdc(), static_cast<float>(general_profile_tier_level.GetLevelIdc()) / 30);

	for (int i = 0; i < maxNumSubLayersMinus1; i++) 
	{
		sub_layer_profile_present_flag[i] = r.Get(1);
		sub_layer_level_present_flag[i]	 = r.Get(1);
	}
	if (maxNumSubLayersMinus1 > 0)
		for (int i = maxNumSubLayersMinus1; i < 8; i++)
			r.Skip(2);	// reserved_zero_2bits[i]

	for (size_t i = 0; i < maxNumSubLayersMinus1; i++)
	{
		if (sub_layer_profile_present_flag[i])
		{ 
			if(!sub_layer_profile_tier_level[i].Decode(r))
				return Warning("-H265ProfileTierLevel::Decode() PTL information for sublayer %zu too short\n", i);
		}
		if (sub_layer_level_present_flag[i])
		{
			sub_layer_profile_tier_level[i].SetLevelIdc(r.Get(8));
			//UltraDebug("-H265: [sub_layer[%zu].level_idc: %d, level: %.02f]\n", i, sub_layer_profile_tier_level[i].GetLevelIdc(), static_cast<float>(sub_layer_profile_tier_level[i].GetLevelIdc()) / 30);
		}
	}

	return true;
}

H265VideoParameterSet::H265VideoParameterSet()
{}

bool H265VideoParameterSet::Decode(const BYTE* buffer,DWORD bufferSize)
{
	//SHould be done otherway, like modifying the BitReader to escape the input NAL, but anyway.. duplicate memory
	Buffer escaped = NalUnescapeRbsp(buffer, bufferSize);
	//Create bit reader
	BitReader r(escaped);

	try 
	{
		vps_id = r.Get(4);
		r.Skip(1); // vps_base_layer_internal_flag
		r.Skip(1); // vps_base_layer_available_flag

		vps_max_layers_minus1		= r.Get(6);
		vps_max_sub_layers_minus1	= r.Get(3);
		vps_temporal_id_nesting_flag	= r.Get(1);
		WORD reserved			= r.Get(16); 

		// vps_reserved_ffff_16bits
		if (reserved != 0xffff)
			return Warning("-H265VideoParameterSet::Decode() vps_reserved_ffff_16bits is not 0xffff\n");

		if (vps_max_sub_layers_minus1 + 1 > HEVCParams::MAX_SUB_LAYERS) 
			return Warning("-H265VideoParameterSet::Decode() vps_max_sub_layers_minus1 out of range: %d\n", vps_max_sub_layers_minus1);

		if (!profile_tier_level.Decode(r, true, vps_max_sub_layers_minus1))
			return Warning("-H265VideoParameterSet::Decode() ptl decode\n");
	}
	catch (std::exception& e)
	{
		return false;
	}

	// skip all the following element decode/parse
	return true;
}

bool H265SeqParameterSet::Decode(const BYTE* buffer,DWORD bufferSize)
{
	//SHould be done otherway, like modifying the BitReader to escape the input NAL, but anyway.. duplicate memory
	Buffer escaped = NalUnescapeRbsp(buffer, bufferSize);
	//Create bit reader
	BitReader r(escaped);

	try
	{
		//Read SPS
		vps_id = r.Get(4);

		const BYTE nuh_layer_id = 0; // sub layers are not supported at the moment
		// profiel_level_tier 
		if (nuh_layer_id == 0)
		{
			max_sub_layers_minus1 = r.Get(3);
		}
		else
		{
			ext_or_max_sub_layers_minus1 = r.Get(3);
		}
		if (max_sub_layers_minus1 > HEVCParams::MAX_SUB_LAYERS - 1)
			return Warning("-H265SeqParameterSet::Decode() sps_max_sub_layers_minus1 out of range: %d\n", max_sub_layers_minus1);

		const bool MultiLayerExtSpsFlag = (nuh_layer_id != 0)	&& (ext_or_max_sub_layers_minus1 ==	7);
		if (!MultiLayerExtSpsFlag)
		{
			temporal_id_nesting_flag = r.Get(1);
			if (!profile_tier_level.Decode(r, true,	max_sub_layers_minus1))
				return Warning("-H265SeqParameterSet::Decode() PTL decode error\n");
		}

		// sps id
		seq_parameter_set_id =	r.GetExpGolomb();
		if (seq_parameter_set_id >= HEVCParams::MAX_SPS_COUNT) 
			return Warning("-H265SeqParameterSet::Decode() SPS id out of range: %d\n", seq_parameter_set_id);

		// chromo_format_idc
		chroma_format_idc = r.GetExpGolomb();
		if (chroma_format_idc > 3U)
			return Error("-H265SeqParameterSet::Decode() chroma_format_idc %d is invalid\n", chroma_format_idc);

		if (chroma_format_idc == 3)
		{
			separate_colour_plane_flag = r.Get(1);
		}

		if(separate_colour_plane_flag)
			chroma_format_idc = 0;

		// width & height in luma
		pic_width_in_luma_samples  = r.GetExpGolomb();
		pic_height_in_luma_samples = r.GetExpGolomb();

		// conformance window
		conformance_window_flag = r.Get(1);
		if (conformance_window_flag) 
		{
			BYTE vert_mult	= hevc_sub_height_c[chroma_format_idc];
			BYTE horiz_mult = hevc_sub_width_c[chroma_format_idc];
			pic_conf_win.left_offset   = r.GetExpGolomb() * horiz_mult;
			pic_conf_win.right_offset  = r.GetExpGolomb() * horiz_mult;
			pic_conf_win.top_offset    = r.GetExpGolomb() *	vert_mult;
			pic_conf_win.bottom_offset = r.GetExpGolomb() *	vert_mult;
		}

		bit_depth_luma_minus8				= r.GetExpGolomb(); 
		bit_depth_chroma_minus8				= r.GetExpGolomb();
		log2_max_pic_order_cnt_lsb_minus4		= r.GetExpGolomb();
		sps_sub_layer_ordering_info_present_flag	= r.Get(1);
	
		for (size_t i = (sps_sub_layer_ordering_info_present_flag ? 0 : max_sub_layers_minus1);
		     i <= max_sub_layers_minus1; i++)
		{
			r.GetExpGolomb();  // sps_max_dec_pic_buffering_minus1[ i ]
			r.GetExpGolomb();  // sps_max_num_reorder_pics[ i ]
			r.GetExpGolomb();  // sps_max_latency_increase_plus1[ i ]
		}
	
		log2_min_luma_coding_block_size_minus3 = r.GetExpGolomb();
		log2_diff_max_min_luma_coding_block_size = r.GetExpGolomb();
		log2_min_luma_transform_block_size_minus2 = r.GetExpGolomb();
		log2_diff_max_min_luma_transform_block_size = r.GetExpGolomb();
	}
	catch (std::exception& e)
	{
		return false;
	}
	
	// Calculate log2PicSizeInCtbsY
	auto MinCbLog2SizeY = log2_min_luma_coding_block_size_minus3 + 3;
	auto CtbLog2SizeY = MinCbLog2SizeY + log2_diff_max_min_luma_coding_block_size;
	auto CtbSizeY = 1 << CtbLog2SizeY;	
	auto PicWidthInCtbsY = ceil(pic_width_in_luma_samples / double(CtbSizeY));
	log2PicSizeInCtbsY = std::ceil(log2(PicWidthInCtbsY));
	
	//OK
	return true;
}

bool H265PictureParameterSet::Decode(const BYTE* buffer,DWORD bufferSize)
{
	//SHould be done otherway, like modifying the BitReader to escape the input NAL, but anyway.. duplicate memory
	Buffer escaped = NalUnescapeRbsp(buffer, bufferSize);
	//Create bit reader
	BitReader r(escaped);

	try
	{
		pps_id = r.GetExpGolomb();
		if (pps_id >= HEVCParams::MAX_PPS_COUNT) 
			return Warning("-H265PictureParameterSet::Decode() PPS id out of range: %d !\n", pps_id);

		sps_id = r.GetExpGolomb();
		if (sps_id >= HEVCParams::MAX_SPS_COUNT) 
			Warning("H265PictureParameterSet::Decode() SPS id out of range: %d !\n", sps_id);

		dependent_slice_segments_enabled_flag	= r.Get(1);
		output_flag_present_flag		= r.Get(1);
		num_extra_slice_header_bits		= r.Get(3);

		sign_data_hiding_flag			= r.Get(1);
		cabac_init_present_flag			= r.Get(1);

		num_ref_idx_l0_default_active_minus1	= r.GetExpGolomb();
		num_ref_idx_l1_default_active_minus1	= r.GetExpGolomb();

		if (num_ref_idx_l0_default_active_minus1 + 1 >= HEVCParams::MAX_REFS ||
			num_ref_idx_l1_default_active_minus1 + 1 >= HEVCParams::MAX_REFS)
			return Warning("-H265PictureParameterSet::Decode() Too many default refs in PPS: %d/%d.\n",
				   num_ref_idx_l0_default_active_minus1 + 1, num_ref_idx_l1_default_active_minus1 + 1);
	}
	catch (std::exception& e)
	{
		return false;
	}
	//OK
	return true;
}

