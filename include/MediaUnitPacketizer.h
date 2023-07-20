#ifndef MEDIAUNITPACKETIZER_H
#define MEDIAUNITPACKETIZER_H

#include "media.h"

class MediaUnitPacketizer
{
public:
	MediaUnitPacketizer(MediaFrame& frame) : frame(frame) {}
	
	virtual void Packetize(size_t unitOffsetInFrameMedia, size_t unitSize) = 0;
	
protected:
	MediaFrame& frame;
};

#endif