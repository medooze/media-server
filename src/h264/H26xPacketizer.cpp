#include "H26xPacketizer.h"

#include "video.h"
#include "h264/h264.h"

H26xPacketizer::H26xPacketizer(VideoCodec::Type codec) :
	RTPPacketizer(MediaFrame::Video, codec)
{}

std::unique_ptr<MediaFrame> H26xPacketizer::ProcessAU(BufferReader& reader)
{
	//UltraDebug("-H26xPacketizer::ProcessAU() | H26x AU [len:%d]\n", reader.GetLeft());
	
	auto frame = std::make_unique<VideoFrame>(static_cast<VideoCodec::Type>(codec), reader.GetLeft());
	
	appender.reset();
	
	NalSliceAnnexB(reader
		, [&](auto nalReader){OnNal(*frame, nalReader);}
	);

	//Check if we have new width and heigth
	if (frame->GetWidth() && frame->GetHeight())
	{
		//Update cache
		width = frame->GetWidth();
		height = frame->GetHeight();
	} else {
		//Update from cache
		frame->SetWidth(width);
		frame->SetHeight(height);
	}

	return frame;
}
