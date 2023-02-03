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
	virtual int DecodePacket(const BYTE *in,DWORD len,int lost,int last);
	virtual int Decode(const BYTE *in,DWORD len);
	virtual int GetWidth()		{ return ctx->width;		};
	virtual int GetHeight()		{ return ctx->height;		};
	virtual const VideoBuffer::shared& GetFrame() { return videoBuffer;	};
	virtual bool  IsKeyFrame()	{ return picture->key_frame;	};
private:
	AVCodec 	*codec = nullptr;
	AVCodecContext	*ctx = nullptr;
	AVFrame		*picture = nullptr;
	
	H264Depacketizer    depacketizer;
	VideoBuffer::shared videoBuffer;
	VideoBufferPool	    videoBufferPool;

};
#endif
