#ifndef _H264ENCODER_H_
#define _H264ENCODER_H_
#include "codecs.h"
#include "video.h"
extern "C" {
#include <x264.h>
}
#include "avcdescriptor.h"

class H264Encoder : public VideoEncoder
{
public:
	H264Encoder(const Properties& properties);
	virtual ~H264Encoder();
	virtual VideoFrame* EncodeFrame(const VideoBuffer::const_shared& videoBuffer);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width,int height);
	virtual int SetFrameRate(int fps,int kbits,int intraPeriod);

private:
	int OpenCodec();
	AVCDescriptor config;
	bool streaming;
	bool annexb;
	x264_t*		enc;
	x264_param_t    params;
	x264_nal_t*	nals;
	x264_picture_t  pic;
	x264_picture_t 	pic_out;
	VideoFrame	frame;
	int curNal;
	int numNals;
	int width;
	int height;
	int bitrate;
	int fps;
	int format;
	int opened;
	int intraPeriod;
	int pts;
	std::string h264ProfileLevelId;
	int threads;
	bool experiment;
	size_t encoded_frame = 0;
};

#endif 
