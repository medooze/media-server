#ifndef RAWFRAME_H
#define RAWFRAME_H

enum class RawFrameMediaType
{
	Config,
	CodedFrames,
	Other
};

class RawFrame
{
public:
	virtual RawFrameMediaType GetRawMediaType() const = 0;
	
	virtual DWORD	GetSize() = 0;
	virtual QWORD GetTimestamp() = 0;
	
	virtual bool IsKeyFrame() const = 0;
	
	virtual BYTE*	GetMediaData()	= 0;
	virtual DWORD	GetMediaSize()	= 0;
};

#endif
