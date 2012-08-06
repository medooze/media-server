#include "log.h"
#include "video.h"
#include "flv1/flv1codec.h"
#include "h263/h263codec.h"
#include "h263/mpeg4codec.h"
#include "h264/h264encoder.h"
#include "h264/h264decoder.h"

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
			return  new H263Decoder1996();
		case VideoCodec::MPEG4:
			return  new Mpeg4Decoder();
		case VideoCodec::H264:
			return  new H264Decoder();
		default:
			Error("Video decoder not found [%d]\n",codec);
	}
	return NULL;
}

VideoEncoder* VideoCodecFactory::CreateEncoder(VideoCodec::Type codec)
{
	Log("-CreateVideoEncoder[%d,%s]\n",codec,VideoCodec::GetNameFor(codec));

	//Depending on the codec
	switch(codec)
	{
		case VideoCodec::SORENSON:
			return new FLV1Encoder(1,31);
		case VideoCodec::H263_1998:
			return new H263Encoder();
		case VideoCodec::H263_1996:
			return  new H263Encoder1996();
		case VideoCodec::MPEG4:
			return  new Mpeg4Encoder(1,31);
		case VideoCodec::H264:
			return  new H264Encoder();
		default:
			Error("Video Encoder not found\n");
	}
	return NULL;
}
