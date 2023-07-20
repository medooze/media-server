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

class H265FrameMediaAppender : public H26xFrameMediaAppender
{
public:
	H265FrameMediaAppender(VideoFrame& frame, unsigned unitLengthSize) :
		H26xFrameMediaAppender(frame, unitLengthSize, std::make_unique<H265NaluPacketizer>(frame))
	{}
};