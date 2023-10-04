#ifndef _WEBPENCODER_H_
#define _WEBPENCODER_H_
#include "codecs.h"
#include "video.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

class WEBPEncoder : public VideoEncoder
{
public:
	WEBPEncoder(const Properties& properties);
	virtual ~WEBPEncoder();
	virtual VideoFrame* EncodeFrame(const VideoBuffer::const_shared& videoBuffer);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width, int height);
	virtual int SetFrameRate(int fps, int kbits, int intraPeriod);

private:
	int OpenCodec();
private:
	const AVCodec* codec	= nullptr;
	AVCodecContext* ctx	= nullptr;
	AVFrame* input		= nullptr;
	VideoFrame	frame;

	int width = 0;
	int height = 0;
	int bitrate = 300000;
	int fps = 30;
	
};

#endif 
