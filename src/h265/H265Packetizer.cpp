#include "H265Packetizer.h"
#include "video.h"

// H.265 uses same stream format (Annex B)
#include "h264/h264nal.h"

void H265Packetizer::OnNal(VideoFrame& frame, BufferReader& reader)
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
		//We consider frames having a VPS/SPS/PPS as intra due to intra frame refresh
		frame.SetIntra(true);
	}

	if (nalUnitType >= 48)
	{
		Error("-H265 got unspecified (>=48) NALU in a context where it is not allowed (header:0x%04x, nalUnitType: %d, nalSize: %d) \n", naluHeader, nalUnitType, nalSize);
		return;
	}

	//Check type
	switch (nalUnitType)
	{
		case HEVC_RTP_NALU_Type::AUD:		//35
		case HEVC_RTP_NALU_Type::EOS:		//36
		case HEVC_RTP_NALU_Type::EOB:		//37
		case HEVC_RTP_NALU_Type::FD:		//38
			//UltraDebug("-H265 Un-defined/implemented NALU, skipping");
			/* undefined */
			return;
		case HEVC_RTP_NALU_Type::VPS:			// 32
		{
			H265VideoParameterSet vps;
			if (!vps.Decode(reader.PeekData(), reader.GetLeft()))
			{
				Error("-H265: Decode of SPS failed!\n");
				break;
			}

			auto& profileTierLevel = vps.GetProfileTierLevel();
			config.SetConfigurationVersion(1);
			config.SetGeneralProfileSpace(profileTierLevel.GetGeneralProfileSpace());
			config.SetGeneralTierFlag(profileTierLevel.GetGeneralTierFlag());
			config.SetGeneralProfileIdc(profileTierLevel.GetGeneralProfileIdc());
			config.SetGeneralProfileCompatibilityFlags(profileTierLevel.GetGeneralProfileCompatibilityFlags());
			config.SetGeneralConstraintIndicatorFlags(profileTierLevel.GetGeneralConstraintIndicatorFlags());
			config.SetGeneralLevelIdc(profileTierLevel.GetGeneralLevelIdc());
			config.SetNALUnitLengthSizeMinus1(OUT_NALU_LENGTH_SIZE - 1);

			//Reset previous VPS only on the 1st VPS in current frame
			if (noVPSInFrame)
			{
				//UltraDebug("-SRTConnection::OnVideoData() | Clear cached VPS\n");
				config.ClearVideoParameterSets();
				noVPSInFrame = false;
			}

			//Add full nal to config
			config.AddVideoParameterSet(nalUnit,nalSize);
			break;
		}
		case HEVC_RTP_NALU_Type::SPS:			// 33
		{
			//Parse sps
			H265SeqParameterSet sps;
			if (!sps.Decode(reader.PeekData(), reader.GetLeft()))
			{
				Error("-H265: Decode of SPS failed!\n");
				break;
			}

			//Set dimensions
			frame.SetWidth(sps.GetWidth());
			frame.SetHeight(sps.GetHeight());

			//UltraDebug("-H265 frame (with cropping) size [width: %d, frame height: %d]\n", sps.GetWidth(), sps.GetHeight());

			//Reset previous SPS only on the 1st SPS in current frame
			if (noSPSInFrame)
			{
				//UltraDebug("-SRTConnection::OnVideoData() | Clear cached SPS\n");
				config.ClearSequenceParameterSets();
				noSPSInFrame = false;
			}

			//Add full nal to config
			config.AddSequenceParameterSet(nalUnit,nalSize);
			break;
		}
		case HEVC_RTP_NALU_Type::PPS:			// 34
			//Reset previous PPS only on the 1st PPS in current frame
			if (noPPSInFrame)
			{
				//UltraDebug("-SRTConnection::OnVideoData() | Clear cached PPS\n");
				config.ClearPictureParameterSets();
				noPPSInFrame = false;
			}

			//Add full nal to config
			config.AddPictureParameterSet(nalUnit, nalSize);
			break;
		default:
			//Debug("-H265 : Nothing to do for this NaluType nalu. Just forwarding it.(nalUnitType: %d, nalSize: %d)\n", nalUnitType, nalSize);
			break;
	}

	// If this frame has been marked as intra (meaning it's either an IDR or a non-IDR
	// with SPS/PPS), and the configuration descriptor is complete, then make sure to
	// attach the configuration descriptor to the VideoFrame (for recording) and to
	// emit the parameter sets.
	//
	// We only need to do this once per frame, and the appropriate time to do it is
	// just before the first slice of the frame:
	if (
		// this NALU is a slice
		((nalUnitType >= 0 && nalUnitType <= 9) || (nalUnitType >= 16 && nalUnitType <= 21)) &&
		// it belongs to an intra frame
		frame.IsIntra() &&
		// no need to do this more than once per frame
		!frame.HasCodecConfig() &&
		// the configuration descriptor is fully populated by now
		config.GetNumOfPictureParameterSets() && config.GetNumOfSequenceParameterSets() && config.GetNumOfVideoParameterSets()
	)
	{
		const uint8_t vpsNum = config.GetNumOfVideoParameterSets();
		// Get total vps size
		// Can improve by adding HEVCDescriptorconfig::GetVPSTotolSize() in media-server
		uint64_t vpsTotalSize = 0;
		for (uint8_t i = 0; i < vpsNum; i++)
			vpsTotalSize += config.GetVideoParameterSetSize(i);

		const uint8_t spsNum = config.GetNumOfSequenceParameterSets();
		// Get total sps size
		// Can improve by adding HEVCDescriptorconfig::GetSPSTotolSize() in media-server
		uint64_t spsTotalSize = 0;
		for (uint8_t i = 0; i < spsNum; i++)
			spsTotalSize += config.GetSequenceParameterSetSize(i);

		const uint8_t ppsNum = config.GetNumOfPictureParameterSets();
		// Get total pps size
		// Can improve by adding HEVCDescriptorconfig::GetPPSTotolSize() in media-server
		uint64_t ppsTotalSize = 0;
		for (uint8_t i = 0; i < ppsNum; i++)
			ppsTotalSize += config.GetPictureParameterSetSize(i);

		//UltraDebug("-SRTConnection::OnVideoData() | SPS [num: %d, size: %d], PPS [num: %d, size: %d]\n", spsNum, spsTotalSize, ppsNum, ppsTotalSize);
		//Allocate enought space to prevent further reallocations
		// preffix num: spsNum + ps_num + 1
		frame.Alloc(frame.GetLength() + nalSize + vpsTotalSize + spsTotalSize + ppsTotalSize + OUT_NALU_LENGTH_SIZE * (vpsNum + spsNum + ppsNum + 1));

		// Add VPS
		for (uint8_t i = 0; i < vpsNum; i++)
			EmitNal(frame, BufferReader(config.GetVideoParameterSet(i), config.GetVideoParameterSetSize(i)));

		// Add SPS
		for (uint8_t i = 0; i < spsNum; i++)
			EmitNal(frame, BufferReader(config.GetSequenceParameterSet(i), config.GetSequenceParameterSetSize(i)));

		// Add PPS
		for (uint8_t i = 0; i < ppsNum; i++)
			EmitNal(frame, BufferReader(config.GetPictureParameterSet(i), config.GetPictureParameterSetSize(i)));

		//Serialize config in to frame
		frame.AllocateCodecConfig(config.GetSize());
		config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
	}

	EmitNal(frame, BufferReader(nalUnit, nalSize));
}

std::unique_ptr<VideoFrame> H265Packetizer::ProcessAU(BufferReader reader)
{
	noPPSInFrame = true;
	noSPSInFrame = true;
	noVPSInFrame = true;

	return H26xPacketizer::ProcessAU(reader);
}