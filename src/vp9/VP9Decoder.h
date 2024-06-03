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
	virtual VideoBuffer::shared GetFrame();
private:
	vpx_codec_ctx_t  decoder;

	VideoBufferPool	    videoBufferPool;

	uint32_t count = 0;
	CircularBuffer<VideoFrame::const_shared, uint32_t, 64> videoFrames;
};
#endif	/* VP9DECODER_H */

