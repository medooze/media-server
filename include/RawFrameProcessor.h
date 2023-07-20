#ifndef RAWFRAMEPROCESSOR_H
#define RAWFRAMEPROCESSOR_H

#include "media.h"
#include "RawFrame.h"

class RawFrameProcessor
{
public:
	virtual std::unique_ptr<MediaFrame> AddFrame(RawFrame* rawFrame) = 0;

};

#endif
