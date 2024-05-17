#ifndef _H264DECODER_H_
#define _H264DECODER_H_
extern "C" {
#include <libavcodec/avcodec.h>
} 
#include "codecs.h"
#include "video.h"
#include "h264depacketizer.h"

class H264Decoder : public VideoDecoder
{
public:
	H264Decoder();
	virtual ~H264Decoder();
	virtual int Decode(const VideoFrame::const_shared& frame);
	virtual VideoBuffer::shared GetFrame();
private:
	const AVCodec*	codec	= nullptr;
	AVCodecContext*	ctx	= nullptr;
	AVFrame*	picture	= nullptr;
	AVPacket*	packet	= nullptr;
	
	VideoBufferPool	    videoBufferPool;
	uint32_t count = 0;
	CircularBuffer<VideoFrame::const_shared, uint32_t, 64> videoFrames;
};
#endif
