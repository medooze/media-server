#ifndef _VIDEO_H_
#define _VIDEO_H_
#include "config.h"
#include "media.h"
#include "codecs.h"
#include "rtp/LayerInfo.h"

struct LayerFrame
{
	DWORD pos	= 0;
	DWORD size	= 0;
	DWORD width	= 0;
	DWORD height	= 0;
	LayerInfo info;
};

class VideoFrame : public MediaFrame
{
public:
	VideoFrame(VideoCodec::Type codec,DWORD size) : MediaFrame(MediaFrame::Video,size)
	{
		//Store codec
		this->codec = codec;
		//Init values
		isIntra = 0;
		width = 0;
		height = 0;
	}
	
	VideoFrame(VideoCodec::Type codec,const std::shared_ptr<Buffer>& buffer) : MediaFrame(MediaFrame::Video,buffer)
	{
		//Store codec
		this->codec = codec;
		//Init values
		isIntra = 0;
		width = 0;
		height = 0;
	}

	virtual MediaFrame* Clone() const
	{
		//Create new one with same data
		VideoFrame *frame = new VideoFrame(codec,buffer);
		//Size
		frame->SetWidth(width);
		frame->SetHeight(height);
		//Set intra
		frame->SetIntra(isIntra);
		//Set clock rate
		frame->SetClockRate(GetClockRate());
		//Set timestamp
		frame->SetTimestamp(GetTimeStamp());
		//Set time
		frame->SetTime(GetTime());
		frame->SetSenderTime(GetSenderTime());
		//Set duration
		frame->SetDuration(GetDuration());
		//If we have disabled the shared buffer for this frame
		if (disableSharedBuffer)
			//Copy data
			frame->AdquireBuffer();
		//Set config
		if (HasCodecConfig()) frame->SetCodecConfig(GetCodecConfigData(),GetCodecConfigSize());
		//Check if it has rtp info
		for (const auto& rtp : rtpInfo)
			//Add it
			frame->AddRtpPacket(rtp.GetPos(),rtp.GetSize(),rtp.GetPrefixData(),rtp.GetPrefixLen());
		//Check if it has layers
		for (const auto& layer : layers)
			//Add it
			frame->AddLayerFrame(layer);
		//Return it
		return (MediaFrame*)frame;
	}
	
	VideoCodec::Type GetCodec() const	{ return codec;			}
	bool  IsIntra()	const			{ return isIntra;		}
	DWORD GetWidth() const			{ return width;			}
	DWORD GetHeight() const			{ return height;		}

	void SetCodec(VideoCodec::Type codec)	{ this->codec = codec;		}
	void SetWidth(DWORD width)		{ this->width = width;		}
	void SetHeight(DWORD height)		{ this->height = height;	}
	void SetIntra(bool isIntra)		{ this->isIntra = isIntra;	}
	
	bool	HasLayerFrames() const				{ return !layers.empty();	}
	const std::vector<LayerFrame>& GetLayerFrames() const	{ return layers;		}
	void AddLayerFrame(const LayerFrame& layer)		{ layers.push_back(layer);	}
	
	void Reset() 
	{
		//Reset media frame
		MediaFrame::Reset();
		//No intra
		SetIntra(false);
		//No new config
		ClearCodecConfig();
		//Clear layers
		layers.clear();
	}
	
private:
	VideoCodec::Type codec;
	bool	isIntra;
	DWORD	width;
	DWORD	height;
	std::vector<LayerFrame> layers;

};

struct VideoBuffer
{
	VideoBuffer() = default;
	VideoBuffer(DWORD width, DWORD height,BYTE* buffer)
	{
		this->width = width;
		this->height = height;
		this->buffer = buffer;
	}
	
	BYTE* GetBufferData() const
	{
		return buffer;
	}
	
	DWORD GetBufferSize() const
	{
		return (width*height*3)/2;
	}
	
	DWORD	width = 0;
	DWORD	height = 0;
	BYTE*	buffer = nullptr;
};

class VideoInput
{
public:
	virtual int   StartVideoCapture(int width,int height,int fps)=0;
	virtual VideoBuffer GrabFrame(DWORD timeout)=0;
	virtual void  CancelGrabFrame()=0;
	virtual int   StopVideoCapture()=0;
};

class VideoOutput
{
public:
	virtual void ClearFrame() = 0;
	virtual int NextFrame(BYTE *pic)=0;
	virtual int SetVideoSize(int width,int height)=0;
};



class VideoEncoder
{
public:
	virtual ~VideoEncoder(){};

	virtual int SetSize(int width,int height)=0;
	virtual VideoFrame* EncodeFrame(BYTE *in,DWORD len)=0;
	virtual int FastPictureUpdate()=0;
	virtual int SetFrameRate(int fps,int kbits,int intraPeriod)=0;
public:
	VideoCodec::Type type;
};

class VideoDecoder
{
public:
	virtual ~VideoDecoder(){};

	virtual int GetWidth()=0;
	virtual int GetHeight()=0;
	virtual int Decode(const BYTE *in,DWORD len) = 0;
	virtual int DecodePacket(const BYTE *in,DWORD len,int lost,int last)=0;
	virtual BYTE* GetFrame()=0;
	virtual bool  IsKeyFrame()=0;
public:
	VideoCodec::Type type;

};

#endif
