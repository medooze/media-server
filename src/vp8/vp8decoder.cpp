#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vp8decoder.h"
#include "vp8.h"
#include "log.h"

VP8Decoder::VP8Decoder() :
	VideoDecoder(VideoCodec::VP8),
	videoBufferPool(2, 4)
{
	//Set flags
	vpx_codec_flags_t flags = 0;
	/**< Postprocess decoded frame */
	flags |= VPX_CODEC_USE_POSTPROC;
	
	//Get codec capatbilities
	vpx_codec_caps_t caps = vpx_codec_get_caps(vpx_codec_vp8_dx());

        /*if (caps & VPX_CODEC_CAP_INPUT_FRAGMENTS)
	{
		Debug("-VPX_CODEC_USE_INPUT_FRAGMENTS enabled\n");
		flags |= VPX_CODEC_USE_INPUT_FRAGMENTS;
	}
        
        if (caps & VPX_CODEC_CAP_ERROR_CONCEALMENT) 
	{
		Debug("-VPX_CODEC_USE_INPUT_FRAGMENTS enabled\n");
                flags |= VPX_CODEC_USE_ERROR_CONCEALMENT;
	}*/
	
	//Init decoder
	if(vpx_codec_dec_init(&decoder, vpx_codec_vp8_dx(), NULL, flags)!=VPX_CODEC_OK)
		Error("Error initing VP8 decoder [error %d:%s]\n", decoder.err, decoder.err_detail ? decoder.err_detail : "(null)");

	vp8_postproc_cfg_t  ppcfg;
	 /**< the types of post processing to be done, should be combination of "vp8_postproc_level" */
	ppcfg.post_proc_flag = VP8_DEMACROBLOCK | VP8_DEBLOCK;
	/**< the strength of deblocking, valid range [0, 16] */
	ppcfg.deblocking_level = 3;
	//Set deblocking  settings
	vpx_codec_control(&decoder, VP8_SET_POSTPROC, &ppcfg);
}

VP8Decoder::~VP8Decoder()
{
	vpx_codec_destroy(&decoder);
}

int VP8Decoder::DecodePacket(const BYTE* data, DWORD size, int lost, int last)
{
	int ret = 1;

	//Add to 
	VideoFrame* frame = (VideoFrame*)depacketizer.AddPayload(data, size);

	//Check last mark
	if (last)
	{
		//If got frame
		if (frame)
		{
			//Check key frame amrk
			isKeyFrame = frame->IsIntra();
			//Decode it
			ret = Decode(frame->GetData(), frame->GetLength());
		} else {
			//Not key frame
			isKeyFrame = false;
		}
		//Reset frame
		depacketizer.ResetFrame();
	}

	//Return ok
	return ret;
}

int VP8Decoder::Decode(const BYTE* data, DWORD size)
{
	//Not key frame
	isKeyFrame = false;
	
	//Decode frame
	vpx_codec_err_t err = vpx_codec_decode(&decoder, data, size, NULL, 0);

	//Check error
	if (err!=VPX_CODEC_OK)
		//Error
		return Error("-VP8Decoder::Decode() | Error decoding VP8 empty [error %d:%s]\n", decoder.err, decoder.err_detail ? decoder.err_detail : "(null)");

	//Check if it is corrupted even if completed
	int corrupted = false;
	if (vpx_codec_control(&decoder, VP8D_GET_FRAME_CORRUPTED, &corrupted)==VPX_CODEC_OK)
		//Set key frame flag
		isKeyFrame =  !(data[0] & 1) && !corrupted;

	//Get image
	vpx_codec_iter_t iter = NULL;
	vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);

	//Check img
	if (!img)
		//Do nothing
		return Error("-VP8Decoder::Decode() | No image\n");

	//Get dimensions
	width = img->d_w;
	height = img->d_h;

	//Set new size in pool
	videoBufferPool.SetSize(width, height);

	//Get new frame
	videoBuffer = videoBufferPool.allocate();

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

	//Copaamos  el Cy
	for (int i = 0; i < std::min<uint32_t>(height, y.GetHeight()); i++)
		memcpy(y.GetData() + i * y.GetStride(), &img->planes[0][i * img->stride[0]], y.GetWidth());

	//Y el Cr y Cb
	for (int i = 0; i < std::min<uint32_t>({ height / 2, u.GetHeight(), v.GetHeight() }); i++)
	{
		memcpy(u.GetData() + i * u.GetStride(), &img->planes[1][i * img->stride[1]], u.GetWidth());
		memcpy(v.GetData() + i * v.GetStride(), &img->planes[2][i * img->stride[2]], v.GetWidth());
	}
		
	//Return ok
	return 1;
}
bool VP8Decoder::IsKeyFrame()
{
	return isKeyFrame;
}
