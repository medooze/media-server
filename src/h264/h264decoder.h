#ifndef _H264DECODER_H_
#define _H264DECODER_H_
extern "C" {
#include <libavcodec/avcodec.h>
} 
#include "codecs.h"
#include "video.h"
#include "h264depacketizer.h"

class H264Decoder : public VideoDecoder
{
public:
	H264Decoder();
	virtual ~H264Decoder();
	virtual int Decode(const VideoFrame::const_shared& frame);
	virtual int GetWidth()		{ return ctx->width;		};
	virtual int GetHeight()		{ return ctx->height;		};
	virtual const VideoBuffer::shared& GetFrame() { return videoBuffer;	};
	virtual bool  IsKeyFrame()	
	{ 
#if AV_FRAME_FLAG_KEY
	return picture->flags & AV_FRAME_FLAG_KEY;
#else
	return picture->key_frame;	
#endif
	}
private:
	const AVCodec*	codec	= nullptr;
	AVCodecContext*	ctx	= nullptr;
	AVFrame*	picture	= nullptr;
	AVPacket*	packet	= nullptr;
	
	VideoBuffer::shared videoBuffer;
	VideoBufferPool	    videoBufferPool;

};
#endif
