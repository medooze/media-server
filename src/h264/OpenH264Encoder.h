#ifndef _OPENH264ENCODER_H_
#define _OPENH264ENCODER_H_
#include <optional>
#include "codecs.h"
#include "video.h"
namespace openh264
{
#include <codec/api/wels/codec_api.h>
}
#include "avcdescriptor.h"


class OpenH264Encoder : public VideoEncoder
{
public:
	OpenH264Encoder(const Properties& properties);
	virtual ~OpenH264Encoder();
	virtual VideoFrame* EncodeFrame(const VideoBuffer::const_shared& videoBuffer);
	virtual int FastPictureUpdate();
	virtual int SetSize(int width, int height);
	virtual int SetFrameRate(int fps, int kbits, int intraPeriod);

private:
	int OpenCodec();
	bool opened = false;
	openh264::ISVCEncoder* encoder = nullptr;

	VideoFrame	frame;
	AVCDescriptor config = {};

	int width = 0;
	int height = 0;
	int bitrate = 0;
	int fps = 0;
	int intraPeriod = 0;
	int pts = 0;

	float ipratio = 1;
	float maxBitrateMultiplier = 1;
	int threads = 0;
	int qMin = 0;
	int qMax = 0;
};

#endif 
