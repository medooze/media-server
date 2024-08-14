#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "AV1Decoder.h"
extern "C" {
#include <libavutil/opt.h>
}

AV1Decoder::AV1Decoder() :
	VideoDecoder(VideoCodec::AV1),
	videoBufferPool(2,4)
{
	// find libdav1d decoder
	codec = avcodec_find_decoder_by_name("libdav1d");

	if(!codec)
	{
		Error("AV1Decoder::AV1Decoder() | No decoder found\n");
		return;
	}

	//Get codec context
	ctx = avcodec_alloc_context3(codec);

	// tilethreads default value is 0, range [0, 64]
	// av_opt_set_int(ctx, "tilethreads", 0, 0);
	// // framethreads default value is 0, range [0, 256]
	// av_opt_set_int(ctx, "framethreads", 0, 0);
	// // select an operating point of the scalable bitstream, default -1, range [-1, 31]
	// av_opt_set_int(ctx, "oppoint", -1, 0);
	// // deprecated?
	// av_opt_set_int(ctx, "filmgrain", 1, 0);
	// // output all spatial layers, default 0
	// av_opt_set_int(ctx, "alllayers", 1, 0);

	//Create packet
	packet = av_packet_alloc();
	//Allocate frame
	picture = av_frame_alloc();

	//Open codec
	avcodec_open2(ctx, codec, NULL);
}

AV1Decoder::~AV1Decoder()
{
	if (ctx)
	{
		avcodec_close(ctx);
		free(ctx);
	}
	if (packet)
		//Release pacekt
		av_packet_free(&packet);
	if (picture)
		//Release frame
		av_frame_free(&picture);
	
}

int AV1Decoder::Decode(const VideoFrame::const_shared& frame)
{
	//Get video frame payload
	packet->data = frame ? (uint8_t*)frame->GetData() : nullptr;
	packet->size = frame ? frame->GetLength() : 0;

	//Store frame num, it will be copied to the decoded avpacket
	ctx->reordered_opaque = count++;
	
	//Store frame reference
	videoFrames.Set(ctx->reordered_opaque, frame);
	
	//Decode it
	if (avcodec_send_packet(ctx, packet) < 0) 
		//Error
		return Error("-AV1Decoder::Decode() Error decoding AV1 packet\n");

	//OK
	return 1;
}

VideoBuffer::shared AV1Decoder::GetFrame()
{
	if (avcodec_receive_frame(ctx, picture) <0)
	{	
		//No frame decoded yet
		return {};
	}
	
	if(ctx->width==0 || ctx->height==0)
	{
		//Warning
		Warning("-AV1Decoder::Decode() | Wrong dimmensions [%d,%d]\n", ctx->width, ctx->height);
		//No frame
		return {};
	}

	//Get original video Frame
	auto ref = videoFrames.Get(picture->reordered_opaque);
	//If not found
	if (!ref)
	{
		//Warning
		Warning("-AV1Decoder::Decode() | Could not found reference frame [reordered:%llu,current:%llu]\n", picture->reordered_opaque, count);
		//No frame
		return {};
	}

	//Set new size in pool
	videoBufferPool.SetSize(ctx->width, ctx->height);

	//Get new frame
	auto videoBuffer = videoBufferPool.allocate();

	//Copy timing info
	CopyPresentedTimingInfo(*ref, videoBuffer);

	videoBuffer->SetInterlaced(picture->interlaced_frame);
	if (picture->sample_aspect_ratio.num!=0)
		//Set pixel aspect ration
		videoBuffer->SetPixelAspectRatio(picture->sample_aspect_ratio.num, picture->sample_aspect_ratio.den);

	switch (picture->color_range)
	{
		case AVCOL_RANGE_MPEG:
			//219*2^(n-8) "MPEG" YUV ranges
			videoBuffer->SetColorRange(VideoBuffer::ColorRange::Partial);
			break;
		case AVCOL_RANGE_JPEG: 
			//2^n-1   "JPEG" YUV ranges
			videoBuffer->SetColorRange(VideoBuffer::ColorRange::Full);
			break;
		default:
			//Unknown
			videoBuffer->SetColorRange(VideoBuffer::ColorRange::Unknown);
	}
	//Get color space
	switch (picture->colorspace)
	{
		case AVCOL_SPC_RGB:
			///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SRGB);
			break;
		case AVCOL_SPC_BT709:
			///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT709);
			break;
		case AVCOL_SPC_BT470BG:
			///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT601);
			break;
		case AVCOL_SPC_SMPTE170M:
			///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SMPTE170);
			break;
		case AVCOL_SPC_SMPTE240M:
			///< functionally identical to above
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SMPTE240);
			break;
		case AVCOL_SPC_BT2020_NCL:
			///< ITU-R BT2020 non-constant luminance system
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT2020);
			break;
		case AVCOL_SPC_BT2020_CL:
			///< ITU-R BT2020 constant luminance system
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT2020);
			break;
		default:
			//Unknown
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::Unknown);
	}
	//Get planes
	Plane& y = videoBuffer->GetPlaneY();
	Plane& u = videoBuffer->GetPlaneU();
	Plane& v = videoBuffer->GetPlaneV();
		
	y.SetData(picture->data[0], ctx->width, ctx->height, picture->linesize[0]);
	u.SetData(picture->data[1], ctx->width / 2, ctx->height / 2, picture->linesize[1]);
	v.SetData(picture->data[2], ctx->width / 2, ctx->height / 2, picture->linesize[2]);

	return videoBuffer;
}

