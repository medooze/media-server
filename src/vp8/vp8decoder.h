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
	virtual int Decode(const VideoFrame::const_shared& frame);
	virtual VideoBuffer::shared GetFrame();
private:
	vpx_codec_ctx_t  decoder;

	VideoBufferPool	    videoBufferPool;

	uint32_t count = 0;
	CircularBuffer<VideoFrame::const_shared, uint32_t, 64> videoFrames;
};
#endif	/* VP8DECODER_H */

