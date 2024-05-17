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
	virtual int Decode(const VideoFrame::const_shared& frame);
	virtual VideoBuffer::shared GetFrame();
private:
	const AVCodec*	codec	= nullptr;
	AVCodecContext*	ctx	= nullptr;
	AVFrame*	picture	= nullptr;
	AVPacket*	packet	= nullptr;
	
	VideoBufferPool	    videoBufferPool;

	Buffer annexb;
	uint32_t count = 0;
	CircularBuffer<VideoFrame::const_shared, uint32_t, 64> videoFrames;
};
#endif
