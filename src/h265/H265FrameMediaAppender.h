#ifndef H265FRAMEMEDIAAPPENDER_H
#define H265FRAMEMEDIAAPPENDER_H

#include "h26x/H26xFrameMediaAppender.h"

class H265FrameMediaAppender : public H26xFrameMediaAppender
{
public:
	H265FrameMediaAppender(VideoFrame& frame, unsigned unitLengthSize) :
		H26xFrameMediaAppender(frame, unitLengthSize, std::make_unique<H265NaluPacketizer>(frame))
	{}
};

#endif
