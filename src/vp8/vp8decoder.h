/* 
 * File:   vp8decoder.h
 * Author: Sergio
 *
 * Created on 13 de noviembre de 2012, 15:57
 */

#ifndef VP8DECODER_H
#define	VP8DECODER_H
#include "config.h"
#include "video.h"
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

class VP8Decoder : public VideoDecoder
{
public:
	VP8Decoder();
	virtual ~VP8Decoder();
	virtual int DecodePacket(BYTE *in,DWORD len,int lost,int last);
	virtual int Decode(BYTE *in,DWORD len);
	virtual int GetWidth()	{return width;};
	virtual int GetHeight()	{return height;};
	virtual BYTE* GetFrame(){return (BYTE *)frame;};
	virtual bool  IsKeyFrame();
private:
	vpx_codec_ctx_t  decoder;
	BYTE*		buffer;
	DWORD		bufLen;
	DWORD 		bufSize;
	BYTE*		frame;
	DWORD		frameSize;
	BYTE		src;
	DWORD		width;
	DWORD		height;
	bool		isKeyFrame;
	bool		completeFrame;
	bool		first;
};
#endif	/* VP8DECODER_H */

