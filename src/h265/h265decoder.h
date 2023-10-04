#ifndef _H265DECODER_H_
#define _H265DECODER_H_
extern "C" {
#include <libavcodec/avcodec.h>
} 
#include "codecs.h"
#include "video.h"
#include "H265Depacketizer.h"

class H265Decoder : public VideoDecoder
{
public:
	H265Decoder();
	virtual ~H265Decoder();
	virtual int DecodePacket(const BYTE *in,DWORD len,int lost,int last);
	virtual int Decode(const BYTE *in,DWORD len);
	virtual int GetWidth()		{ return ctx->width;		};
	virtual int GetHeight()		{ return ctx->height;		};
	virtual const VideoBuffer::shared& GetFrame() { return videoBuffer;		};
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
	
	H265Depacketizer    depacketizer;
	VideoBuffer::shared videoBuffer;
	VideoBufferPool	    videoBufferPool;

};
#endif
