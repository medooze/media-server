#include "H265Packetizer.h"
#include "video.h"

// H.265 uses same stream format (Annex B)
#include "h264/h264nal.h"

void OnH265Nal(VideoFrame& frame, HEVCDescriptor &config, BufferReader& reader)
{
	//Return if current NAL is empty
	if (!reader.GetLeft())
		return;

	auto nalSize = reader.GetLeft();
	auto nalUnit = reader.PeekData();

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

	auto naluHeader 		= reader.Get2();
	BYTE nalUnitType		= (naluHeader >> 9) & 0b111111;
	BYTE nuh_layer_id		= (naluHeader >> 3) & 0b111111;
	BYTE nuh_temporal_id_plus1	= naluHeader & 0b111;

	if (nuh_layer_id != 0)
	{
		Error("-H265: nuh_layer_id(%d) is not 0, which we don't support yet!\n", nuh_layer_id);
		return;
	}

	UltraDebug("-H265 [NAL header:0x%04x,type:%d,layer_id:%d, temporal_id:%d, size:%d]\n", naluHeader, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1, nalSize);

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
			Error("-H265 TODO: non implemented yet, need update to rfc7798, return nullptr (header:0x%04x, nalUnitType: %d, nalSize: %d) \n", naluHeader, nalUnitType, nalSize);
			return;
		case HEVC_RTP_NALU_Type::UNSPEC49_FU: 
			Error("-H265 TODO: non implemented yet, need update to rfc7798, return nullptr (header:0x%04x, nalUnitType: %d, nalSize: %d)\n", naluHeader, nalUnitType, nalSize);
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
			if (vps.Decode(reader.PeekData(), reader.GetLeft()))
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
			if (sps.Decode(reader.PeekData(), reader.GetLeft()))
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

	EmitNal(frame, BufferReader(nalUnit, nalSize));
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
