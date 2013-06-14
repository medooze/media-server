#include "log.h"
#include "video.h"
#include "flv1/flv1codec.h"
#include "h263/h263codec.h"
#include "h263/mpeg4codec.h"
#include "h264/h264encoder.h"
#include "h264/h264decoder.h"
#include "vp8/vp8decoder.h"
#include "vp8/vp8encoder.h"
#include "vp6/vp6decoder.h"

VideoDecoder* VideoCodecFactory::CreateDecoder(VideoCodec::Type codec)
{
	Log("-CreateVideoDecoder[%d,%s]\n",codec,VideoCodec::GetNameFor(codec));

	//Depending on the codec
	switch(codec)
	{
		case VideoCodec::SORENSON:
			return new FLV1Decoder();
		case VideoCodec::H263_1998:
			return new H263Decoder();
		case VideoCodec::H263_1996:
			return new H263Decoder1996();
		case VideoCodec::MPEG4:
			return new Mpeg4Decoder();
		case VideoCodec::H264:
			return new H264Decoder();
		case VideoCodec::VP6:
			return new VP6Decoder();
		case VideoCodec::VP8:
			return new VP8Decoder();
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
		case VideoCodec::SORENSON:
			return new FLV1Encoder(properties);
		case VideoCodec::H263_1998:
			return new H263Encoder(properties);
		case VideoCodec::H263_1996:
			return new H263Encoder1996(properties);
		case VideoCodec::MPEG4:
			return new Mpeg4Encoder(properties);
		case VideoCodec::H264:
			return new H264Encoder(properties);
		case VideoCodec::VP8:
			return new VP8Encoder(properties);
		default:
			Error("Video Encoder not found\n");
	}
	return NULL;
}