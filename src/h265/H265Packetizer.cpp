#include "H265Packetizer.h"
#include "video.h"

// H.265 uses same stream format (Annex B)
#include "h264/H26xNal.h"

H265Packetizer::H265Packetizer() : H26xPacketizer(VideoCodec::H265)
{

}

void H265Packetizer::EmitNal(VideoFrame& frame, BufferReader nal)
{
	auto naluHeader = nal.Peek2();
	BYTE nalUnitType = (naluHeader >> 9) & 0b111111;
	BYTE nuh_layer_id = (naluHeader >> 3) & 0b111111;
	BYTE nuh_temporal_id_plus1 = naluHeader & 0b111;

	const uint16_t nalHeaderFU = ((uint16_t)(HEVC_RTP_NALU_Type::UNSPEC49_FU) << 9)
		| ((uint16_t)(nuh_layer_id) << 3)
		| ((uint16_t)(nuh_temporal_id_plus1));
	std::string fuPrefix = {static_cast<char>((nalHeaderFU & 0xff00) >> 8), static_cast<char>(nalHeaderFU & 0xff), (char)nalUnitType};
	H26xPacketizer::EmitNal(frame, nal, fuPrefix, HEVCParams::RTP_NAL_HEADER_SIZE);
}

void H265Packetizer::OnNal(VideoFrame& frame, BufferReader& reader, std::optional<bool>& frameEnd)
{
	//@todo Set frameEnd as per slice header
	
	//UltraDebug("-H265Packetizer::OnNal()\n");
	//Return if current NAL is empty
	if (!reader.GetLeft())
		return;

	auto nalSize = reader.GetLeft();
	auto nalUnit = reader.PeekData();

	//Check length
	if (nalSize < HEVCParams::RTP_NAL_HEADER_SIZE)
		//Exit
		return;

	BYTE nalUnitType;
	BYTE nuh_layer_id;
	BYTE nuh_temporal_id_plus1;
 	if(!H265DecodeNalHeader(nalUnit, nalSize, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1))
	{
		Error("-H265Packetizer::OnNal() | Failed to decode H265 Nal Header!\n");
		return;
	}
	reader.Skip(HEVCParams::RTP_NAL_HEADER_SIZE);

	if (nuh_layer_id != 0)
	{
		Error("-H265Packetizer::OnNal() | nuh_layer_id(%d) is not 0, which we don't support yet!\n", nuh_layer_id);
		return;
	}

	//Check if IDR/SPS/PPS, set Intra
	if (H265IsIntra(nalUnitType))
	{
		//We consider frames having a VPS/SPS/PPS as intra due to intra frame refresh
		frame.SetIntra(true);
	}

	if (nalUnitType >= HEVC_RTP_NALU_Type::UNSPEC48_AP)
	{
		Error("-H265Packetizer::OnNal() | got unspecified (>=48) NALU in a context where it is not allowed (nalUnitType: %u, nalSize: %zu) \n", nalUnitType, nalSize);
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
				Error("-H265Packetizer::OnNal() | Decode of SPS failed!\n");
				return;
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
			return;
		}
		case HEVC_RTP_NALU_Type::SPS:			// 33
		{
			//Parse sps
			auto localSps = std::make_unique<H265SeqParameterSet>();
			if (!localSps->Decode(reader.PeekData(), reader.GetLeft()))
			{
				Error("-H265Packetizer::OnNal() | Decode of SPS failed!\n");
				break;
			}
			sps = std::move(localSps);

			//Set dimensions
			frame.SetWidth(sps->GetWidth());
			frame.SetHeight(sps->GetHeight());

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
			return;
		}
		case HEVC_RTP_NALU_Type::PPS:			// 34
		{
			//Parse pps
			auto localPps = std::make_unique<H265PictureParameterSet>();
			if (!localPps->Decode(reader.PeekData(), reader.GetLeft()))
			{
				Error("-H265Packetizer::OnNal() | Decode of PPS failed!\n");
				break;
			}
			pps = std::move(localPps);
		
			//Reset previous PPS only on the 1st PPS in current frame
			if (noPPSInFrame)
			{
				//UltraDebug("-SRTConnection::OnVideoData() | Clear cached PPS\n");
				config.ClearPictureParameterSets();
				noPPSInFrame = false;
			}

			//Add full nal to config
			config.AddPictureParameterSet(nalUnit, nalSize);
			return;
		}
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
		// this NALU is a slice or SEI
		((nalUnitType <= HEVC_RTP_NALU_Type::RASL_R)
		 || (nalUnitType >= HEVC_RTP_NALU_Type::BLA_W_LP && nalUnitType <= HEVC_RTP_NALU_Type::CRA_NUT)
		 || (nalUnitType == HEVC_RTP_NALU_Type::SEI_PREFIX) || (nalUnitType == HEVC_RTP_NALU_Type::SEI_SUFFIX))
		// it belongs to an intra frame
		&& frame.IsIntra()
		// no need to do this more than once per frame
		&& !frame.HasCodecConfig()
		// the configuration descriptor is fully populated by now
		&& config.GetNumOfPictureParameterSets() && config.GetNumOfSequenceParameterSets() && config.GetNumOfVideoParameterSets()
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
	
	if (((nalUnitType <= HEVC_RTP_NALU_Type::RASL_R) || 
	    (nalUnitType >= HEVC_RTP_NALU_Type::BLA_W_LP && nalUnitType <= HEVC_RTP_NALU_Type::CRA_NUT)) &&
	    pps && sps)
	{
		H265SliceHeader header;
		if (header.Decode(reader.PeekData(), reader.GetLeft(), nalUnitType, *pps, *sps))
		{
			frame.SetBFrame(header.GetSliceType() == 0);
		}
	}

	EmitNal(frame, BufferReader(nalUnit, nalSize));
}

std::unique_ptr<MediaFrame> H265Packetizer::ProcessAU(BufferReader& reader)
{
	UltraDebug("-H265Packetizer::ProcessAU()| H265 AU [len:%zd]\n", reader.GetLeft());
	noPPSInFrame = true;
	noSPSInFrame = true;
	noVPSInFrame = true;

	return H26xPacketizer::ProcessAU(reader);
}
