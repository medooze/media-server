/* 
 * File:   vp8encoder.h
 * Author: Sergio
 *
 * Created on 13 de noviembre de 2012, 15:57
 */

#ifndef VP8ENCODER_H
#define	VP8ENCODER_H
#include "config.h"
#include "video.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

class VP8Encoder : public VideoEncoder
{
public:
	VP8Encoder(const Properties& properties);
	virtual ~VP8Encoder();
	virtual VideoFrame* EncodeFrame(BYTE *in,DWORD len);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width,int height);
	virtual int SetFrameRate(int fps,int kbits,int intraPeriod);
private:
	int OpenCodec();
private:
	vpx_codec_ctx_t		encoder;
	vpx_codec_enc_cfg_t	config;
	vpx_image_t*		pic;
	VideoFrame*		frame;
	bool forceKeyFrame;
	int width;
	int height;
	int numPixels;
	int bitrate;
	int fps;
	int format;
	int opened;
	int intraPeriod;
	int pts;
	int num;
};

#endif	/* VP8ENCODER_H */

