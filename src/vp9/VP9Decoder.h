#ifndef VP9DECODER_H
#define	VP9DECODER_H
#include "config.h"
#include "video.h"
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#include "VP9Depacketizer.h"

class VP9Decoder : public VideoDecoder
{
public:
	VP9Decoder();
	virtual ~VP9Decoder();
	virtual int DecodePacket(const BYTE *in,DWORD len,int lost,int last);
	virtual int Decode(const BYTE *in,DWORD len);
	virtual int GetWidth()	{return width;};
	virtual int GetHeight()	{return height;};
	virtual BYTE* GetFrame(){return (BYTE *)frame;};
	virtual bool  IsKeyFrame();
private:
	vpx_codec_ctx_t  decoder;
	VP9Depacketizer  depacketizer;
	BYTE*		frame;
	DWORD		frameSize;
	DWORD		width;
	DWORD		height;
	bool		isKeyFrame;
	bool		completeFrame;
	bool		first;
};
#endif	/* VP9DECODER_H */

