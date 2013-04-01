#ifndef _MPEG4CODEC_H_
#define _MPEG4CODEC_H_
extern "C" {
#include <libavcodec/avcodec.h>
} 
#include "codecs.h"
#include "video.h"

class Mpeg4Decoder : public VideoDecoder
{
public:
	Mpeg4Decoder();
	virtual ~Mpeg4Decoder();
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
	static DWORD 	bufSize;
	BYTE*		frame;
	DWORD		frameSize;
	BYTE		src;
};

class Mpeg4Encoder : public VideoEncoder
{
public:
	Mpeg4Encoder(const Properties& properties);
	virtual ~Mpeg4Encoder();

	virtual VideoFrame* EncodeFrame(BYTE *in,DWORD len);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width,int height);
	virtual int SetFrameRate(int fps,int kbits,int intraPeriod);

private:
	int OpenCodec();

	AVCodec 	*codec;
	AVCodecContext	*ctx;
	AVFrame		*picture;
	int		bitrate;
	int		fps;
	int		format;
	int		opened;
	int		qMin;
	int		qMax;
	int		intraPeriod;
	VideoFrame* 	frame;
	DWORD		bufLen;
	DWORD		bufIni;
	static DWORD	bufSize;
};

#endif
