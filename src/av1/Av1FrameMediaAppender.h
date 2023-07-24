#ifndef AV1FRAMEMEDIAAPPENDER_H
#define AV1FRAMEMEDIAAPPENDER_H

#include "FrameMediaAppender.h"
#include "av1/Av1ObuPacketizer.h"

class Av1FrameMediaAppender : public FrameMediaAppender
{
public:
	Av1FrameMediaAppender(VideoFrame& frame) : 
		FrameMediaAppender(frame, std::make_unique<Av1ObuPacketizer>(frame))
	{
	}

protected:
	virtual void AddPrefix(size_t unitSize) override
	{
		// @todo Implement this function
	}
};

#endif

