#ifndef AUDIOCODECFACTORY_H
#define AUDIOCODECFACTORY_H
#include "audio.h"

class AudioCodecFactory
{
public:
	static AudioDecoder* CreateDecoder(AudioCodec::Type codec);
	static AudioEncoder* CreateEncoder(AudioCodec::Type codec);
	static AudioEncoder* CreateEncoder(AudioCodec::Type codec, const Properties &properties);
};

#endif /* AUDIOCODECFACTORY_H */

