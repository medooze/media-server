
#include "h264/H264FrameMediaAppender.h"
#include "h265/H265FrameMediaAppender.h"
#include "av1/Av1FrameMediaAppender.h"

std::unique_ptr<FrameMediaAppender> FrameMediaAppender::Create(VideoFrame& frame, unsigned unitLengthSize)
{
	switch (frame.GetCodec())
	{
	case VideoCodec::H264:
		return std::make_unique<H264FrameMediaAppender>(frame, unitLengthSize);
	case VideoCodec::H265:
		return std::make_unique<H265FrameMediaAppender>(frame, unitLengthSize);
	case VideoCodec::AV1:
		return std::make_unique<Av1FrameMediaAppender>(frame);
	default:
		return nullptr;
	}	
}