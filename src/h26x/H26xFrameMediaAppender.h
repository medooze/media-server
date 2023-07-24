#ifndef H26XFRAMEAPPENDER_H
#define H26XFRAMEAPPENDER_H

#include "FrameMediaAppender.h"
#include "h264/H264NaluPacketizer.h"
#include "h265/H265NaluPacketizer.h"

class H26xFrameMediaAppender : public FrameMediaAppender
{
public:	
	H26xFrameMediaAppender(VideoFrame& frame, unsigned unitLengthSize, std::unique_ptr<MediaUnitPacketizer> packetizer) : 
		FrameMediaAppender(frame, std::move(packetizer)),
		unitLengthField(unitLengthSize)
	{
	}

protected:

	virtual void AddPrefix(size_t unitSize) override
	{
		//Set size
		setN(unitLengthField.size(), unitLengthField.data(), 0, unitSize);
		
		//Append nal size header
		frame.AppendMedia(unitLengthField.data(), unitLengthField.size());
	}	

private:
	
	std::vector<uint8_t> unitLengthField;
	
};

#endif
