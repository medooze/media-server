#ifndef _VIDEO_H_
#define _VIDEO_H_
#include <optional>
#include "config.h"
#include "media.h"
#include "codecs.h"
#include "rtp/LayerInfo.h"
#include "VideoBuffer.h"
#include "VideoBufferPool.h"

struct LayerFrame
{
	uint32_t pos	= 0;
	uint32_t size	= 0;
	uint32_t width	= 0;
	uint32_t height	= 0;
	LayerInfo info;
};

class VideoFrame : public MediaFrame
{
public:
	using shared = std::shared_ptr<VideoFrame>;
	using const_shared = std::shared_ptr<const VideoFrame>;
public:
	VideoFrame(VideoCodec::Type codec) : MediaFrame(MediaFrame::Video)
	{
		//Store codec
		this->codec = codec;
	}

	VideoFrame(VideoCodec::Type codec,uint32_t size) : MediaFrame(MediaFrame::Video,size)
	{
		//Store codec
		this->codec = codec;
	}
	
	VideoFrame(VideoCodec::Type codec,const std::shared_ptr<Buffer>& buffer) : MediaFrame(MediaFrame::Video,buffer)
	{
		//Store codec
		this->codec = codec;
	}

	VideoFrame(VideoCodec::Type codec, Buffer&& buffer) : MediaFrame(MediaFrame::Video, std::move(buffer))
	{
		//Store codec
		this->codec = codec;
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
		//Set B frame
		frame->SetBFrame(isBFrame);
		//Set clock rate
		frame->SetClockRate(GetClockRate());
		//Set timestamp
		frame->SetTimestamp(GetTimeStamp());
		//Set time
		frame->SetTime(GetTime());
		frame->SetSenderTime(GetSenderTime());
		frame->SetTimestampSkew(GetTimestampSkew());
		//Set duration
		frame->SetDuration(GetDuration());

		// Original time meta-data
		frame->SetTSClockRate(GetTSClockRate());
		frame->SetPTS(GetPTS());
		frame->SetDTS(GetDTS());

		//Set CVO
		if (cvo) frame->SetVideoOrientation(*cvo);
		//Copy target bitrate and fps
		frame->SetTargetBitrate(targetBitrate);
		frame->SetTargetFps(targetFps);
		// SSRC
		frame->SetSSRC(GetSSRC());
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
	uint32_t GetWidth() const		{ return width;			}
	uint32_t GetHeight() const		{ return height;		}

	void SetCodec(VideoCodec::Type codec)	{ this->codec = codec;		}
	void SetWidth(uint32_t width)		{ this->width = width;		}
	void SetHeight(uint32_t height)		{ this->height = height;	}
	void SetIntra(bool isIntra)		{ this->isIntra = isIntra;	}

	bool	HasLayerFrames() const				{ return !layers.empty();	}
	const std::vector<LayerFrame>& GetLayerFrames() const	{ return layers;		}
	void AddLayerFrame(const LayerFrame& layer)		{ layers.push_back(layer);	}
	
	void SetVideoOrientation(const VideoOrientation cvo)		{ this->cvo = cvo;	}
	std::optional<VideoOrientation> GetVideoOrientation() const	{ return this->cvo;	}

	void SetTargetBitrate(uint32_t targetBitrate)	{ this->targetBitrate = targetBitrate;	}
	uint32_t GetTargetBitrate() const		{ return this->targetBitrate;		}

	void SetTargetFps(uint32_t targetFps)		{ this->targetFps = targetFps;		}	
	uint32_t GetTargetFps() const			{ return this->targetFps;		}

	void SetBFrame(bool isBFrame) { this->isBFrame = isBFrame; }
	bool IsBFrame() const { return isBFrame; }

	void SetTSClockRate(uint32_t v) { tsClockRate = v;}
	void SetPTS(uint64_t v) { pts = v;}
	void SetDTS(uint64_t v) { dts = v;}
	uint32_t GetTSClockRate() const { return tsClockRate;}
	uint64_t GetPTS() const { return pts;}
	uint64_t GetDTS() const { return dts;}

	void Reset() 
	{
		//Reset media frame
		MediaFrame::Reset();
		//No intra
		SetIntra(false);		
		//Reset B frame
		SetBFrame(false);
		//No new config
		ClearCodecConfig();
		//Clear layers
		layers.clear();

		tsClockRate = 0;
		pts = 0;
		dts = 0;
	}
	
private:
	VideoCodec::Type codec;
	bool	 isIntra	= false;
	bool	 isBFrame	= false;
	uint32_t width		= 0;
	uint32_t height		= 0;
	uint32_t targetBitrate	= 0;
	uint32_t targetFps	= 0;
	std::vector<LayerFrame> layers;
	std::optional<VideoOrientation> cvo;

	// We will store the original PTS/DTS here separate from the base timestamp
	// This allows the code to change the base timestamp as needed but not forget 
	// the original PTS/DTS
	uint32_t tsClockRate = 0;
	uint64_t pts = 0;
	uint64_t dts = 0;
};


class VideoInput
{
public:
	virtual ~VideoInput() = default;

	virtual int   StartVideoCapture(uint32_t width, uint32_t height, uint32_t fps)=0;
	virtual VideoBuffer::const_shared GrabFrame(uint32_t timeout)=0;
	virtual void  CancelGrabFrame()=0;
	virtual int   StopVideoCapture()=0;
};

class VideoOutput
{
public:
	virtual ~VideoOutput() = default;

	virtual void ClearFrame() = 0;

	// Returns the current occupancy of the frame buffer queue
	virtual size_t NextFrame(const VideoBuffer::const_shared& videoBuffer)=0;
};



class VideoEncoder
{
public:
	virtual ~VideoEncoder() = default;

	virtual int SetSize(int width,int height)=0;
	virtual VideoFrame* EncodeFrame(const VideoBuffer::const_shared& videoBuffer)=0;
	virtual int FastPictureUpdate()=0;
	virtual int SetFrameRate(int fps,int kbits,int intraPeriod)=0;
public:
	VideoCodec::Type type;
};

class VideoDecoder
{
public:
	VideoDecoder(VideoCodec::Type type) : type(type)
	{};
	virtual ~VideoDecoder() = default;
	virtual int Decode(const VideoFrame::const_shared &frame) = 0;
	virtual VideoBuffer::shared GetFrame() = 0;
public:
	VideoCodec::Type type;

};

inline void CopyTimingInfo(const VideoFrame::const_shared& videoFrame, VideoBuffer::shared& videoBuffer)
{
	if (!videoFrame)
		return;

	videoBuffer->SetTime(videoFrame->GetTime());
	videoBuffer->SetTimestamp(videoFrame->GetTimestamp());
	videoBuffer->SetClockRate(videoFrame->GetClockRate());

	if (videoFrame->GetSenderTime())
		videoBuffer->SetSenderTime(videoFrame->GetSenderTime());

	videoBuffer->SetTSClockRate(videoFrame->GetTSClockRate());
	videoBuffer->SetPTS(videoFrame->GetPTS());
	videoBuffer->SetDTS(videoFrame->GetDTS());
}

#endif
