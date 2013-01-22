#ifndef _H264DECODER_H_
#define _H264DECODER_H_
extern "C" {
#include <libavcodec/avcodec.h>
} 
#include "codecs.h"
#include "video.h"

class H264Decoder : public VideoDecoder
{
public:
	H264Decoder();
	virtual ~H264Decoder();
	virtual int DecodePacket(BYTE *in,DWORD len,int lost,int last);
	virtual int Decode(BYTE *in,DWORD len);
	virtual int GetWidth()		{ return ctx->width;		};
	virtual int GetHeight()		{ return ctx->height;		};
	virtual BYTE* GetFrame()	{ return (BYTE *)frame;		};
	virtual bool  IsKeyFrame()	{ return picture->key_frame;	};
private:
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	AVFrame		*picture;
	BYTE*		buffer;
	DWORD		bufLen;
	DWORD 		bufSize;
	BYTE*		frame;
	DWORD		frameSize;
	BYTE		src;
};
#endif
