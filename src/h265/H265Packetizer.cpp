#include "H265Packetizer.h"
#include "video.h"

// H.265 uses same stream format (Annex B)
#include "h264/h264nal.h"

void OnH265Nal(VideoFrame& frame, HEVCDescriptor &config, BufferReader& reader)
{
	//Return if current NAL is empty
	if (!reader.GetLeft())
		return;

	//Get nal size
	uint32_t nalSize = reader.GetLeft();
	//Get nal unit
	const uint8_t* nalUnit = reader.PeekData();

	BYTE nalHeaderPrefix[4]; // set as AnnexB or not
	DWORD pos;
	//Check length
	if (nalSize < HEVCParams::RTP_NAL_HEADER_SIZE)
		//Exit
		return;

	/* 
	 *   +-------------+-----------------+
	 *   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
	 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *   |F|   Type    |  LayerId  | TID |
	 *   +-------------+-----------------+
	 *
	 * F must be 0.
	 */

	BYTE nalUnitType = (nalUnit[0] & 0x7e) >> 1;
	BYTE nuh_layer_id = ((nalUnit[0] & 0x1) << 5) + ((nalUnit[1] & 0xf8) >> 3);
	BYTE nuh_temporal_id_plus1 = nalUnit[1] & 0x7;

	if (nuh_layer_id != 0)
	{
		Error("-H265: nuh_layer_id(%d) is not 0, which we don't support yet!\n", nuh_layer_id);
		return;
	}

	//Get nal data
	const BYTE* nalData = nalUnit + HEVCParams::RTP_NAL_HEADER_SIZE;

	UltraDebug("-H265 [NAL header:0x%02x%02x,type:%d,layer_id:%d, temporal_id:%d, size:%d]\n", nalUnit[0], nalUnit[1], nalUnitType, nuh_layer_id, nuh_temporal_id_plus1, nalSize);

	//Check if IDR/SPS/PPS, set Intra
	if ((nalUnitType == HEVC_RTP_NALU_Type::IDR_W_RADL)
		|| (nalUnitType == HEVC_RTP_NALU_Type::IDR_N_LP)
		|| (nalUnitType == HEVC_RTP_NALU_Type::VPS)
		|| (nalUnitType == HEVC_RTP_NALU_Type::SPS)
		|| (nalUnitType == HEVC_RTP_NALU_Type::PPS))
	{
		frame.SetIntra(true);
	}

	//Check type
	switch (nalUnitType)
	{
		case HEVC_RTP_NALU_Type::AUD:		//35
		case HEVC_RTP_NALU_Type::EOS:		//36
		case HEVC_RTP_NALU_Type::EOB:		//37
		case HEVC_RTP_NALU_Type::FD:		//38
			Warning("-H265 Un-defined/implemented NALU, skipping");
			/* undefined */
			return;
		case HEVC_RTP_NALU_Type::UNSPEC48_AP:	//48 
			Error("-H265 TODO: non implemented yet, need update to rfc7798, return nullptr (nalUnit[0]: 0x%02x, nalUnitType: %d, nalSize: %d) \n", nalUnit[0], nalUnitType, nalSize);
			return;
		case HEVC_RTP_NALU_Type::UNSPEC49_FU: 
			Error("-H265 TODO: non implemented yet, need update to rfc7798, return nullptr (nalUnit[0]: 0x%02x, nalUnitType: %d, nalSize: %d)\n", nalUnit[0], nalUnitType, nalSize);
			return;
			/* 4.4.1.  Single NAL Unit Packets */
			/* 
					0                   1                   2                   3
				    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				   |           PayloadHdr          |      DONL (conditional)       |
				   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				   |                                                               |
				   |                  NAL unit payload data                        |
				   |                                                               |
				   |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				   |                               :...OPTIONAL RTP padding        |
				   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			*/
			/* @Zita Liao: sprop-max-don-diff is considered to be absent right now (so no DONL). Need to extract from SDP */
			/* the entire payload is the output buffer */
		case HEVC_RTP_NALU_Type::VPS:			// 32
		{
			H265VideoParameterSet vps;
			if (vps.Decode(nalData, nalSize-1))
			{
				if(config.GetConfigurationVersion() == 0)
				{
					auto& profileTierLevel = vps.GetProfileTierLevel();
					config.SetConfigurationVersion(1);
					config.SetGeneralProfileSpace(profileTierLevel.GetGeneralProfileSpace());
					config.SetGeneralTierFlag(profileTierLevel.GetGeneralTierFlag());
					config.SetGeneralProfileIdc(profileTierLevel.GetGeneralProfileIdc());
					config.SetGeneralProfileCompatibilityFlags(profileTierLevel.GetGeneralProfileCompatibilityFlags());
					config.SetGeneralConstraintIndicatorFlags(profileTierLevel.GetGeneralConstraintIndicatorFlags());
					config.SetGeneralLevelIdc(profileTierLevel.GetGeneralLevelIdc());
					config.SetNALUnitLengthSizeMinus1(sizeof(nalHeaderPrefix) - 1);
				}
				//Add full nal to config
				config.AddVideoParameterSet(nalUnit,nalSize);
			}
			else
			{
				Error("-H265: Decode of SPS failed!\n");
			}
			break;
		}
		case HEVC_RTP_NALU_Type::SPS:			// 33
		{
			//Parse sps
			H265SeqParameterSet sps;
			if (sps.Decode(nalData,nalSize-1))
			{
				//Set dimensions
				frame.SetWidth(sps.GetWidth());
				frame.SetHeight(sps.GetHeight());
	
				//UltraDebug("-H265 frame (with cropping) size [width: %d, frame height: %d]\n", sps.GetWidth(), sps.GetHeight());

				//Add full nal to config
				config.AddSequenceParameterSet(nalUnit,nalSize);
			}
			else
			{
				Error("-H265: Decode of SPS failed!\n");
			}
			break;
		}
		case HEVC_RTP_NALU_Type::PPS:			// 34
			//Add full nal to config
			config.AddPictureParameterSet(nalUnit,nalSize);
			break;
		default:
			//Debug("-H265 : Nothing to do for this NaluType nalu. Just forwarding it.(nalUnitType: %d, nalSize: %d)\n", nalUnitType, nalSize);
			break;
	}

	//Set size
	set4(nalHeaderPrefix, 0, nalSize); // would not be AnnexB here

	//Append data
	frame.AppendMedia(nalHeaderPrefix, sizeof(nalHeaderPrefix));
	//Append data and get current post
	pos = frame.AppendMedia(nalUnit, nalSize);
	//Add RTP packet
	if (nalSize >= RTPPAYLOADSIZE)
	{
		// use FU:
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |    PayloadHdr (Type=49)       |   FU header   | DONL (cond)   |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
		// | DONL (cond)   |                                               |
		// |-+-+-+-+-+-+-+-+                                               |
		// |                         FU payload                            |
		// |                                                               |
		// |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |                               :...OPTIONAL RTP padding        |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// FU header:
		// +---------------+
		// |0|1|2|3|4|5|6|7|
		// +-+-+-+-+-+-+-+-+
		// |S|E|  FuType   |
		// +---------------+

		//Debug("-H265: nalSize(%d) is larger than RTPPAYLOADSIZE (%d)\n", nalSize, RTPPAYLOADSIZE);

		//nalHeader = {PayloadHdr, FU header}, we don't suppport DONL yet
		const uint16_t nalHeaderFU = ((uint16_t)(HEVC_RTP_NALU_Type::UNSPEC49_FU) << 9)
									| ((uint16_t)(nuh_layer_id) << 3)
									| ((uint16_t)(nuh_temporal_id_plus1)); 
		uint8_t S = 1, E = 0;
		auto ConstructFUHeader = [&]() {return (uint8_t)(S << 7 | (E << 6) | (nalUnitType & 0b11'1111));};
		std::array<uint8_t, 3> nalHeader = {(uint8_t)((nalHeaderFU & 0xff00) >> 8), (uint8_t)(nalHeaderFU & 0xff), ConstructFUHeader()};

		//Pending uint8_ts
		uint32_t pending = nalSize;
		//Skip payload nal header
		pending -= HEVCParams::RTP_NAL_HEADER_SIZE;
		pos += HEVCParams::RTP_NAL_HEADER_SIZE;

		//Split it
		while (pending)
		{
			int len = std::min<uint32_t>(RTPPAYLOADSIZE - nalHeader.size(), pending);
			//Read it
			pending -= len;
			//If all added
			if (!pending) //Set end mark
			{ 
				E = 1;
			}
			nalHeader[2] = ConstructFUHeader();
			//Add packetization
			frame.AddRtpPacket(pos, len, nalHeader.data(), nalHeader.size());

			//Debug("-H265: FU headers: nalHeader[0][1][2] = 0x%02x, 0x%02x, 0x%02x\n", nalHeader[0], nalHeader[1], nalHeader[2]);

			if (S == 1) // set start mark to 0 after first FU packet
			{
				S = 0;
			}
			//Move start
			pos += len;
		}
	}
	else
	{
		frame.AddRtpPacket(pos, nalSize, nullptr, 0);
	}
}

std::unique_ptr<VideoFrame> H265Packetizer::ProcessAU(BufferReader reader)
{
	//UltraDebug("-H265Packetizer::ProcessAU() | H265 AU [len:%d]\n", reader.GetLeft());

	//Alocate enought data
	auto frame = std::make_unique<VideoFrame>(VideoCodec::H265, reader.GetSize());

	NalSliceAnnexB(std::move(reader), [&](auto reader) {
		OnH265Nal(*frame, config, reader);
	});

	//Set config size
	frame->AllocateCodecConfig(config.GetSize());
	//Serialize
	config.Serialize(frame->GetCodecConfigData(), frame->GetCodecConfigSize());

	//Check if we have new width and heigth
	if (frame->GetWidth() && frame->GetHeight())
	{
		//Update cache
		width = frame->GetWidth();
		height = frame->GetHeight();
	} else {
		//Update from cache
		frame->SetWidth(width);
		frame->SetHeight(height);
	}

	return frame;
}
