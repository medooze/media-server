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
	virtual VideoFrame* EncodeFrame(const VideoBuffer::const_shared& videoBuffer);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width,int height);
	virtual int SetFrameRate(int fps,int kbits,int intraPeriod);
private:
	int OpenCodec();
private:
	vpx_codec_ctx_t		encoder = {};
	vpx_codec_enc_cfg_t	config = {};
	vpx_image_t*		pic = {};
	VideoFrame		frame;
	bool forceKeyFrame = false;
	int width = 0;
	int height = 0;
	DWORD numPixels = 0;
	int bitrate = 0;
	int fps = 0;
	int format = 0;
	int opened = 0;
	int intraPeriod = 0;
	int pts = 0;
	int num = 0;
	int threads = 0;
	int cpuused = 0;
	int maxKeyFrameBitratePct = 0;
	int deadline = 0;
	int errorResilientPartitions = 0;
	int dropFrameThreshold = 0;
	vpx_rc_mode endUsage = vpx_rc_mode::VPX_CBR;
	int minQuantizer = 0;
	int maxQuantizer = 0;
	int undershootPct = 0;
	int overshootPct = 0;
	int bufferSize = 0;
	int bufferInitialSize = 0;
	int bufferOptimalSize = 0;
	int staticThreshold = 0;
	int noiseReductionSensitivity = 0;

};

#endif	/* VP8ENCODER_H */

