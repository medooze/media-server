#ifndef VP8DECODER_H
#define	VP8DECODER_H
#include "config.h"
#include "video.h"
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#include "vp8depacketizer.h"

class VP8Decoder : public VideoDecoder
{
public:
	VP8Decoder();
	virtual ~VP8Decoder();
	virtual int DecodePacket(const BYTE *data,DWORD size,int lost,int last);
	virtual int Decode(const BYTE *data,DWORD size);
	virtual int GetWidth()	{return width;};
	virtual int GetHeight()	{return height;};
	virtual const VideoBuffer::shared& GetFrame() { return videoBuffer; };
	virtual bool  IsKeyFrame();
private:
	vpx_codec_ctx_t  decoder;

	VP8Depacketizer    depacketizer;
	VideoBuffer::shared videoBuffer;
	VideoBufferPool	    videoBufferPool;

	DWORD		width = 0;
	DWORD		height = 0;
	bool		isKeyFrame = false;
};
#endif	/* VP8DECODER_H */

