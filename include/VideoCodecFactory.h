#ifndef VIDEOCODECFACTORY_H
#define VIDEOCODECFACTORY_H
#include "video.h"

class VideoCodecFactory
{
public:
	static VideoDecoder* CreateDecoder(VideoCodec::Type codec);
	static VideoEncoder* CreateEncoder(VideoCodec::Type codec);
	static VideoEncoder* CreateEncoder(VideoCodec::Type codec, const Properties &properties);
};

#endif /* VIDEOCODECFACTORY_H */

