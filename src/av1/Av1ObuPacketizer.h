
#ifndef AV1OBUPACKETIZER_H
#define AV1OBUPACKETIZER_H

#include "MediaUnitPacketizer.h"

class Av1ObuPacketizer : public MediaUnitPacketizer 
{
public:
	Av1ObuPacketizer(VideoFrame& frame) : MediaUnitPacketizer(frame) {};	

	virtual void Packetize(size_t unitOffsetInFrameMedia, size_t unitSize) override
	{
		// @todo Implement this function
	}
};

#endif