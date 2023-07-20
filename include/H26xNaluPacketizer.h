#ifndef H26XNALUPACKETIZER_H
#define H26XNALUPACKETIZER_H

#include "MediaUnitPacketizer.h"
#include "video.h"

#include <algorithm>

class H26xNaluPacketizer : public MediaUnitPacketizer 
{
protected:
	
	H26xNaluPacketizer(VideoFrame& frame) : MediaUnitPacketizer(frame) {};	

	void Packetize(std::vector<uint8_t>& firstFragmentationHeader, size_t unitOffsetInFrameMedia, size_t payloadSize)
	{
		while (payloadSize)
		{
			//Get fragment size
			auto fragSize = std::min(payloadSize, (RTPPAYLOADSIZE-firstFragmentationHeader.size()));
			//Check if it is last
			if (fragSize==payloadSize)
				//Set end bit
				firstFragmentationHeader.back() |= 0x40;
				
			//Add rpt packet info
			frame.AddRtpPacket(unitOffsetInFrameMedia,fragSize,firstFragmentationHeader.data(),firstFragmentationHeader.size());
			
			//Remove start bit
			firstFragmentationHeader.back() &= 0x7F;
			//Next
			unitOffsetInFrameMedia += fragSize;
			//Remove size
			payloadSize -= fragSize;
		}
	}
};


#endif
