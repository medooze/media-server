#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "h264decoder.h"
extern "C" {
#include <libavutil/opt.h>
}

H264Decoder::H264Decoder() :
	VideoDecoder(VideoCodec::H264),
	videoBufferPool(2,4)
{
	//Open libavcodec
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);

	//Check
	if(!codec)
	{
		Error("H264Decoder::H264Decoder() | No decoder found\n");
		return;
	}

	//Get codec context
	ctx = avcodec_alloc_context3(codec);

	//Use not annex b
	av_opt_set_int(ctx, "is_avc", 1, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(ctx, "nal_length_size", 4, AV_OPT_SEARCH_CHILDREN);

	//Create packet
	packet = av_packet_alloc();
	//Allocate frame
	picture = av_frame_alloc();

	//Open codec
	avcodec_open2(ctx, codec, NULL);
}

H264Decoder::~H264Decoder()
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

int H264Decoder::Decode(const VideoFrame::const_shared& frame)
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
		return Warning("-H264Decoder::Decode() Error decoding H264 packet\n");

	//OK
	return 1;
}

VideoBuffer::shared H264Decoder::GetFrame()
{

	//Check if we got any decoded frame
	if (avcodec_receive_frame(ctx, picture) <0)
	{	
		//No frame decoded yet
		return {};
	}
	
	if(ctx->width==0 || ctx->height==0)
	{
		//Warning
		Warning("-H264Decoder::Decode() | Wrong dimmensions [%d,%d]\n", ctx->width, ctx->height);
		//No frame
		return {};
	}

	//Get original video Frame
	auto ref = videoFrames.Get(picture->reordered_opaque);

	//If not found
	if (!ref)
	{
		//Warning
		Warning("-H264Decoder::Decode() | Could not found reference frame [reordered:%llu,current:%llu]\n", picture->reordered_opaque, count);
		//No frame
		return {};
	}


	//Set new size in pool
	videoBufferPool.SetSize(ctx->width, ctx->height);

	//Get new frame
	auto videoBuffer = videoBufferPool.allocate();

	//Copy timing info
	CopyPresentedTimingInfo(*ref, videoBuffer);

	//Set interlaced flags
	videoBuffer->SetInterlaced(picture->interlaced_frame);

	//IF the pixel aspect ratio is valid
	if (picture->sample_aspect_ratio.num!=0)
		//Set pixel aspect ration
		videoBuffer->SetPixelAspectRatio(picture->sample_aspect_ratio.num, picture->sample_aspect_ratio.den);

	//Set color range
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
		
	//Copy data to each plane
	y.SetData(picture->data[0], ctx->width, ctx->height, picture->linesize[0]);
	u.SetData(picture->data[1], ctx->width / 2, ctx->height / 2, picture->linesize[1]);
	v.SetData(picture->data[2], ctx->width / 2, ctx->height / 2, picture->linesize[2]);

	//OK
	return videoBuffer;
}

