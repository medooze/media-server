#include "AV1LayerSelector.h"

#include "av1/AV1.h"
#include "av1/Obu.h"

AV1LayerSelector::AV1LayerSelector() : DependencyDescriptorLayerSelector(VideoCodec::AV1)
{
}

std::vector<LayerInfo> AV1LayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	if (packet->GetCodec() != VideoCodec::AV1) return {};
	
	auto infos = DependencyDescriptorLayerSelector::GetLayerIds(packet);

	// We will parse the resolution only if it is not set yet
	if (packet->GetWidth() != 0 && packet->GetHeight() != 0) return infos;
	
	if (packet->GetMediaLength() == 0) return infos;
	
	BufferReader reader(packet->GetMediaData(), packet->GetMediaLength());

	// Get aggregation header
	RtpAv1AggreationHeader header;

	if (!header.Parse(reader)) return {};

	//If startsNewCodedVideoSequence
	if (header.field.N)
		//It is intra
		packet->SetKeyFrame(true);

	// Skip fragmented first element
	if (header.field.Z) return infos;
	
	// We only parse the first OBU. The size field would not present when only one element in the packet
	uint32_t obuSize = reader.GetLeft();
	if (header.field.W != 1)
	{
		obuSize = reader.DecodeLev128();
	}
	
	// Not enough data
	if (!reader.Assert(obuSize)) return infos;
	
	// The sequence header is always the first element in the packet if present, so we just need to check the first one.
	ObuHeader obuHeader;
	if (obuHeader.Parse(reader) && obuHeader.type == ObuType::ObuSequenceHeader)
	{
		SequenceHeaderObu sho;
		if (sho.Parse(reader))
		{
			//Set dimensions
			packet->SetWidth(sho.max_frame_width_minus_1 + 1);
			packet->SetHeight(sho.max_frame_height_minus_1 + 1);
		}
	}
	
	return infos;
}