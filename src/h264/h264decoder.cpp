#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "h264decoder.h"

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
	if (!frame)
		return 0;

	//Get video frame payload
	const BYTE* data  = frame->GetData();
	DWORD size = frame->GetLength();

	//Set data
	packet->data = (uint8_t*)data;
	packet->size = size;

	//Decode it
	if (avcodec_send_packet(ctx, packet) < 0)
		//Error
		return Warning("-H264Decoder::Decode() Error decoding H264 packet\n");

	//Check if we got any decoded frame
	if (avcodec_receive_frame(ctx, picture) <0)
		//No frame decoded yet
		return 1;
	
	if(ctx->width==0 || ctx->height==0)
		return Error("-H264Decoder::Decode() | Wrong dimmensions [%d,%d]\n",ctx->width,ctx->height);

	//Set new size in pool
	videoBufferPool.SetSize(ctx->width, ctx->height);

	//Get new frame
	videoBuffer = videoBufferPool.allocate();

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
		
	//Copaamos  el Cy
	for (uint32_t i = 0; i < std::min<uint32_t>(ctx->height, y.GetHeight()); i++)
		memcpy(y.GetData() + i * y.GetStride(), &picture->data[0][i * picture->linesize[0]], y.GetWidth());

	//Y el Cr y Cb
	for (uint32_t i = 0; i < std::min<uint32_t>({ ctx->height / 2, u.GetHeight(), v.GetHeight() }); i++)
	{
		memcpy(u.GetData() + i * u.GetStride(), &picture->data[1][i * picture->linesize[1]], u.GetWidth());
		memcpy(v.GetData() + i * v.GetStride(), &picture->data[2][i * picture->linesize[2]], v.GetWidth());
	}

	//OK
	return 1;
}

