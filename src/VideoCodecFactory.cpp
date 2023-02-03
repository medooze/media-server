#include "log.h"
#include "VideoCodecFactory.h"
#include "h264/h264encoder.h"
#include "h264/h264decoder.h"
#include "vp8/vp8decoder.h"
#include "vp8/vp8encoder.h"
#include "vp9/VP9Decoder.h"
#include "jpeg/JPEGEncoder.h"

VideoDecoder* VideoCodecFactory::CreateDecoder(VideoCodec::Type codec)
{
	Log("-CreateVideoDecoder[%d,%s]\n",codec,VideoCodec::GetNameFor(codec));

	//Depending on the codec
	switch(codec)
	{
		case VideoCodec::H264:
			return new H264Decoder();
		case VideoCodec::VP8:
			return new VP8Decoder();
		case VideoCodec::VP9:
			return new VP9Decoder();
		default:
			Error("Video decoder not found [%d]\n",codec);
	}
	return NULL;
}

VideoEncoder* VideoCodecFactory::CreateEncoder(VideoCodec::Type codec)
{
	//Empty properties
	Properties properties;

	//Create codec
	return CreateEncoder(codec,properties);
}


VideoEncoder* VideoCodecFactory::CreateEncoder(VideoCodec::Type codec,const Properties& properties)
{
	Log("-CreateVideoEncoder[%d,%s]\n",codec,VideoCodec::GetNameFor(codec));

	//Depending on the codec
	switch(codec)
	{
		case VideoCodec::H264:
			return new H264Encoder(properties);
		case VideoCodec::VP8:
			return new VP8Encoder(properties);
		case VideoCodec::JPEG:
			return new JPEGEncoder(properties);
		default:
			Error("Video Encoder not found\n");
	}
	return NULL;
}
