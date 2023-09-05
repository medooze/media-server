#ifndef _H264ENCODER_H_
#define _H264ENCODER_H_
#include "codecs.h"
#include "video.h"
extern "C" {
#include <x264.h>
}
#include "avcdescriptor.h"

static const char * const x264_level_names[] = { "1", "1b", "1.1", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1", "3.2",
                                                "4", "4.1", "4.2", "5", "5.1", "5.2", "6", "6.1", "6.2", 0}; // need 0 as last element to comfort to x264_xxx_names
static const std::string h264UnSet = "UnSet";

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
	bool IsParamsValid(const char * const options[], const std::string& input) const;
	int LevelNumberToLevelIdc(const std::string& levelNumber) const;
	std::string LevelInUse() const { return level == h264UnSet? "3.1":level;}
	std::string ProfileInUse() const { return profile == h264UnSet? "baseline":profile;}
	std::string PresetInUse() const { return preset == h264UnSet? "medium":preset;}
	std::string TuneInUse() const { return tune == h264UnSet? "zerolatency":tune;}

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
	std::string profile = h264UnSet;
	std::string level = h264UnSet;
	std::string preset = h264UnSet;
	std::string tune = h264UnSet;
	float ipratio;
	float ratetol;
	int threads;
};

#endif 
