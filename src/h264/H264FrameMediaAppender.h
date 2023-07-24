#ifndef H264FRAMEMEDIAAPPENDER_H
#define H264FRAMEMEDIAAPPENDER_H

#include "h26x/H26xFrameMediaAppender.h"
class H264FrameMediaAppender : public H26xFrameMediaAppender
{
public:
	H264FrameMediaAppender(VideoFrame& frame, unsigned unitLengthSize) :
		H26xFrameMediaAppender(frame, unitLengthSize, std::make_unique<H264NaluPacketizer>(frame))
	{}
	
	virtual void AppendUnit(const uint8_t* unitData, size_t unitSize) override
	{
		//Skip fill data nalus for h264
		if (unitData[0] == 12) return;
			
		H26xFrameMediaAppender::AppendUnit(unitData, unitSize);
	}
};

#endif
