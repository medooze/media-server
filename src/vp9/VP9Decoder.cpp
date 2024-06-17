/* 
 * File:   vp8decoder.cpp
 * Author: Sergio
 * 
 * Created on 13 de noviembre de 2012, 15:57
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "VP9Decoder.h"
#include "log.h"

/***********************
* VP9Decoder
*	Consturctor
************************/
VP9Decoder::VP9Decoder() :
	VideoDecoder(VideoCodec::VP9),
	videoBufferPool(2, 4)
{
	//Codec config and flags
	vpx_codec_flags_t flags = 0;
	vpx_codec_dec_cfg_t cfg = {};
	
	//Set number of decoding threads
	//cfg.threads = std::min(number_of_cores, kMaxNumTiles4kVideo);

	//Init decoder
	if(vpx_codec_dec_init(&decoder, vpx_codec_vp9_dx(), &cfg, flags)!=VPX_CODEC_OK)
		Error("Error initing VP9 decoder [error %d:%s]\n",decoder.err,decoder.err_detail);
}

/***********************
* ~VP9Decoder
*	Destructor
************************/
VP9Decoder::~VP9Decoder()
{
	vpx_codec_destroy(&decoder);

}

int VP9Decoder::Decode(const VideoFrame::const_shared& frame)
{
	//Get video frame payload
	const BYTE* data = frame ? frame->GetData() : nullptr;
	DWORD size = frame ? frame->GetLength() : 0;

	//Set frame counter as private data
	uint64_t user_priv = count++;
	//Store frame reference
	videoFrames.Set(user_priv, frame);

	//Decode
	vpx_codec_err_t err = vpx_codec_decode(&decoder, data,size, (void*)user_priv,0);
	
	//Check error
	if (err!=VPX_CODEC_OK)
		//Error
		return Error("-VP9Decoder::Decode() | Error decoding VP9 frame [error %d:%s]\n",decoder.err,decoder.err_detail);
	//Ok
	return 1;
}

VideoBuffer::shared VP9Decoder::GetFrame()
{

	//Ger image
	vpx_codec_iter_t iter = NULL;
	vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);

	//Check img
	if (!img)
	{
		//Warning
		Warning("-VP9Decoder::Decode() | No image\n");
		//No frame
		return {};
	}

	//Get original video Frame
	auto ref = videoFrames.Get((uint64_t)img->user_priv);

	//If not found
	if (!ref)
	{
		//Warning
		Warning("-VP9Decoder::Decode() | Could not found reference frame [reordered:%llu,current:%llu]\n", (uint64_t)img->user_priv, count);
		//No frame
		return {};
	}

	//Set new size in pool
	videoBufferPool.SetSize(img->d_w, img->d_h);

	//Get new frame
	auto videoBuffer = videoBufferPool.allocate();
	CopyPresentedTimingInfo(*ref, videoBuffer);
	//Set color range
	switch (img->range)
	{
		case VPX_CR_STUDIO_RANGE:
			//219*2^(n-8) "MPEG" YUV ranges
			videoBuffer->SetColorRange(VideoBuffer::ColorRange::Partial);
			break;
		case VPX_CR_FULL_RANGE:
			//2^n-1   "JPEG" YUV ranges
			videoBuffer->SetColorRange(VideoBuffer::ColorRange::Full);
			break;
	}

	//Get color space
	switch (img->cs)
	{
		case VPX_CS_SRGB:
			///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SRGB);
			break;
		case VPX_CS_BT_709:
			///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT709);
			break;
		case VPX_CS_BT_601:
			///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT601);
			break;
		case VPX_CS_SMPTE_170:
			///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SMPTE170);
			break;
		case VPX_CS_SMPTE_240:
			///< functionally identical to above
			videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SMPTE240);
			break;
		case VPX_CS_BT_2020:
			///< ITU-R BT2020 non-constant luminance system
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
	y.SetData(img->planes[0], img->d_w, img->d_h, img->stride[0]);
	u.SetData(img->planes[1], img->d_w / 2, img->d_h / 2, img->stride[1]);
	v.SetData(img->planes[2], img->d_w / 2, img->d_h / 2, img->stride[2]);

	//Return ok
	return videoBuffer;
}
