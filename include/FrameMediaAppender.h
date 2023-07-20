#ifndef FRAMEMEDIAAPPENDER_H
#define FRAMEMEDIAAPPENDER_H

#include "video.h"
#include "MediaUnitPacketizer.h"
#include "codecs.h"

class FrameMediaAppender
{
public:	
	FrameMediaAppender(VideoFrame& frame, std::unique_ptr<MediaUnitPacketizer> packetizer) : 
		frame(frame),
		packetizer(std::move(packetizer))
	{
	}

	virtual void AppendUnit(const uint8_t* unitData, size_t unitSize)
	{
		// Add prefix for recording
		AddPrefix(unitSize);
		
		//Append nal
		auto offset = frame.AppendMedia(unitData, unitSize);
		
		//Crete rtp packet
		packetizer->Packetize(offset, unitSize);
	}
	
	static std::unique_ptr<FrameMediaAppender> Create(VideoFrame& frame, unsigned unitLengthSize);
	
protected:
	
	virtual void AddPrefix(size_t unitSize) = 0;

	MediaFrame& frame;
	
	std::unique_ptr<MediaUnitPacketizer> packetizer;
};

#endif