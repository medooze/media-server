#ifndef H265NALUPACKETIZER_H
#define H265NALUPACKETIZER_H

#include "h265.h"
#include "H26xNaluPacketizer.h"

class H265NaluPacketizer : public H26xNaluPacketizer
{
public:
	H265NaluPacketizer(VideoFrame& frame) : H26xNaluPacketizer(frame) {};

	virtual void Packetize(size_t unitOffsetInFrameMedia, size_t unitSize) override
	{
		if (unitSize <= RTPPAYLOADSIZE)
		{
			frame.AddRtpPacket(unitOffsetInFrameMedia, unitSize, nullptr, 0);
			return;
		}
		
		const uint8_t *data = frame.GetData() + unitOffsetInFrameMedia;
		
		auto naluHeader = get2(data, 0);
		BYTE nalUnitType		= (naluHeader >> 9) & 0b111111;
		BYTE nuh_layer_id		= (naluHeader >> 3) & 0b111111;
		BYTE nuh_temporal_id_plus1	= naluHeader & 0b111;

		const uint16_t nalHeaderFU = ((uint16_t)(HEVC_RTP_NALU_Type::UNSPEC49_FU) << 9)
							| ((uint16_t)(nuh_layer_id) << 3)
							| ((uint16_t)(nuh_temporal_id_plus1)); 
		
		std::vector<uint8_t> fragmentationHeader(3);
		set2(fragmentationHeader.data(), 0, nalHeaderFU);
		fragmentationHeader[2] = nalUnitType | 0x80;
		
		//Skip original header
		H26xNaluPacketizer::Packetize(fragmentationHeader, unitOffsetInFrameMedia + 2, unitSize - 2);
	}
};

#endif
