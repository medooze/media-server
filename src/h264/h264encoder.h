#ifndef _H264ENCODER_H_
#define _H264ENCODER_H_
#include <optional>
#include "codecs.h"
#include "video.h"
extern "C" {
#include <x264.h>
}
#include "avcdescriptor.h"

static const char * const x264_level_names[] = { "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1", "3.2",
                                                "4", "4.1", "4.2", "5", "5.1", "5.2", "6", "6.1", "6.2", 0}; // need 0 as last element to comfort to x264_xxx_names

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
	int LevelNumberToLevelIdc(const std::string& levelNumber) const;
	std::string GetLevelInUse() const { return level.value_or("3.1");}
	std::string GetProfileInUse() const { return profile.value_or("baseline");}
	std::string GetPresetInUse() const { return preset.value_or("medium");}
	std::string GetTuneInUse() const { return tune.value_or("zerolatency");}

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
	std::optional<std::string> profile ;
	std::optional<std::string> level   ;
	std::optional<std::string> preset  ;
	std::optional<std::string> tune    ;
	float ipratio = -1.0F;
	float ratetol = 1.0F;
	int bufferSizeInFrames = 1;
	int threads;
};

#endif 
