#ifndef _H263CODEC_H_
#define _H263CODEC_H_
extern "C" {
#include <libavcodec/avcodec.h>
}

#include "h263.h"
#include "codecs.h"
#include "video.h"
#include <list>

class H263Encoder : public VideoEncoder
{
public:
	H263Encoder(const Properties& properties);
	virtual ~H263Encoder();
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
	int		intraPeriod;
	VideoFrame	*frame;
};

class H263Decoder : public VideoDecoder
{
public:
	H263Decoder();
	virtual ~H263Decoder();
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

class H263Encoder1996 : public VideoEncoder
{
private:
	struct RTPInfo
	{
		RTPInfo(BYTE* data,int size,int mb)
		{
			this->data = data;
			this->size = size;
			this->mb = mb;
		}
		BYTE* data;
		int size;
		int mb;
	};
public:
	H263Encoder1996(const Properties& properties);
	virtual ~H263Encoder1996();
	virtual VideoFrame* EncodeFrame(BYTE *in,DWORD len);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width,int height);
	virtual int SetFrameRate(int fps,int kbits,int intraPeriod);
private:
	int OpenCodec();

private:
	AVCodec 	*codec;
	AVCodecContext	*ctx;
	AVFrame		*picture;
	int		bitrate;
	int		fps;
	int		format;
	int		opened;
	int		intraPeriod;
	VideoFrame	*frame;
	WORD		num;
	H263RFC2190Paquetizer paquetizer;
};

class H263Decoder1996 : public VideoDecoder
{
public:
	H263Decoder1996();
	virtual ~H263Decoder1996();
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

#endif
