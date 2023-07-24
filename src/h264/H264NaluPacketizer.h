#ifndef H264NALUPACKETIZER_H
#define H264NALUPACKETIZER_H

#include "h26x/H26xNaluPacketizer.h"

class H264NaluPacketizer : public H26xNaluPacketizer
{
public:
	H264NaluPacketizer(VideoFrame& frame) : H26xNaluPacketizer(frame) {};

	virtual void Packetize(size_t unitOffsetInFrameMedia, size_t unitSize) override
	{
		if (unitSize <= RTPPAYLOADSIZE)
		{
			frame.AddRtpPacket(unitOffsetInFrameMedia, unitSize, nullptr, 0);
			return;
		}	
			
		const uint8_t *nal = frame.GetData() + unitOffsetInFrameMedia;
		
		//Get original nal type
		BYTE nalUnitType = nal[0] & 0x1f;
		//The fragmentation unit A header, S=1
		/* +---------------+---------------+
		 * |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 * |F|NRI|   28    |S|E|R| Type	   |
		 * +---------------+---------------+
		*/
		std::vector<uint8_t> fragmentationHeader = {
			(BYTE)((nal[0] & 0xE0) | 28),
			(BYTE)(0x80 | nalUnitType)
		};
		
		//Skip original header
		H26xNaluPacketizer::Packetize(fragmentationHeader, unitOffsetInFrameMedia + 1, unitSize - 1);
	}
};

#endif
