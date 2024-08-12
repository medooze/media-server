#ifndef AV1ENCODER_H
#define	AV1ENCODER_H
#include "config.h"
#include "video.h"
#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "av1/AV1.h"

class AV1Encoder : public VideoEncoder
{
public:
	AV1Encoder(const Properties& properties);
	virtual ~AV1Encoder();
	virtual VideoFrame* EncodeFrame(const VideoBuffer::const_shared& videoBuffer);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width, int height);
	virtual int SetFrameRate(int fps, int kbits, int intraPeriod);

private:
	int OpenCodec();


private:
	aom_codec_ctx_t		encoder = {};
	aom_codec_enc_cfg_t	config = {};
	aom_image_t* pic = {};
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
	aom_rc_mode endUsage = aom_rc_mode::AOM_CBR;
	int minQuantizer = 0;
	int maxQuantizer = 0;
	int undershootPct = 0;
	int overshootPct = 0;
	int bufferSize = 0;
	int bufferInitialSize = 0;
	int bufferOptimalSize = 0;
	int noiseReductionSensitivity = 0;
	int aqMode = 0;
	

};

#endif	/* AV1ENCODER_H */

