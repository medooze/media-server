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
	virtual int Decode(const VideoFrame::const_shared& frame);
	virtual int GetWidth() { return width; };
	virtual int GetHeight() { return height; };
	virtual const VideoBuffer::shared& GetFrame() { return videoBuffer; };
	virtual bool  IsKeyFrame();
private:
	vpx_codec_ctx_t  decoder;

	VideoBuffer::shared videoBuffer;
	VideoBufferPool	    videoBufferPool;

	DWORD		width = 0;
	DWORD		height = 0;
	bool		isKeyFrame = false;
};
#endif	/* VP9DECODER_H */

