#include <VideoBufferScaler.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
}

int VideoBufferScaler::Resize(const VideoBuffer::const_shared& input, const VideoBuffer::shared& output, bool keepAspectRatio)
{
	//Get planes
	const Plane& iy = input->GetPlaneY();
	const Plane& iu = input->GetPlaneU();
	const Plane& iv = input->GetPlaneV();

	Plane& oy = output->GetPlaneY();
	Plane& ou = output->GetPlaneU();
	Plane& ov = output->GetPlaneV();

	// Set values
	uint32_t srcWidth	= input->GetWidth();
	uint32_t srcHeight	= input->GetHeight();
	uint32_t dstWidth	= output->GetWidth();
	uint32_t dstHeight	= output->GetHeight();
	uint32_t resizeWidth	= dstWidth;
	uint32_t resizeHeight	= dstHeight;
	uint32_t offsetX	= 0;
	uint32_t offsetY	= 0;

	//Fill output with black
	output->Fill(0, (BYTE)-128, (BYTE)-128);

	//Check aspect ratio flag
	if (false)//keepAspectRatio)
	{
		//Fill output with black
		output->Fill(0, (BYTE)-128, (BYTE)-128);

		//Get ratios
		double srcRatio = (double)srcWidth / srcHeight;
		double dstRatio = (double)dstWidth / dstHeight;
		//Compare ratios
		if (srcRatio < dstRatio)
		{
			//Put vertical scroll bars
			// -------------------------------------
			// |       |                   |       |
			// |       |                   |       |
			// |       |                   |       |
			// |       |                   |       |
			// |       |                   |       |
			// ------------------------------------
			//Recaultulate weight
			resizeWidth = std::ceil(dstHeight * srcRatio);
			offsetX = (dstWidth - resizeWidth) / 2;
		} else if (srcRatio > dstRatio) {
			//Put horizontal scroll bars
			// -------------------------------------
			// |                                   |
			// -------------------------------------
			// |                                   |
			// |                                   |
			// |                                   |
			// |                                   |
			// ------------------------------------
			// |                                   |
			// -------------------------------------
			//Recaultulate weight
			resizeHeight = std::ceil(dstWidth / srcRatio);
			offsetY = (dstHeight - resizeHeight) / 2;
		}
	}

	//Resize context
	SwsContext* resizeCtx = sws_getContext(
		srcWidth,
		srcHeight,
		AV_PIX_FMT_YUV420P,
		resizeWidth,
		resizeHeight,
		AV_PIX_FMT_YUV420P,
		SWS_BICUBIC,
		nullptr,
		nullptr,
		nullptr
	);

	if (!resizeCtx)
		// Exit 
		return Error("-VideoBufferScaler::Resize() | Couldn't get sws context\n");

	
	// Set source info
	const BYTE* srcData[3] = {
		iy.GetData(),
		iu.GetData(),
		iv.GetData()
	};
	int srcStride[3] = {
		iy.GetStride(),
		iu.GetStride(),
		iv.GetStride()
	};
	//Get destination info
	BYTE* dstData[3] = {
		oy.GetData() + offsetX   + offsetY     * oy.GetStride() ,
		ou.GetData() + offsetX/2 + (offsetY/2) * ou.GetStride() ,
		ov.GetData() + offsetX/2 + (offsetY/2) * ov.GetStride()
	};
	int dstStride[3] = {
		oy.GetStride(),
		ou.GetStride(),
		ov.GetStride()
	};

	// Resize frame 
	if (sws_scale(resizeCtx, srcData, srcStride, 0, srcHeight, dstData, dstStride)<0)
	{
		//Free ctx
		sws_freeContext(resizeCtx);
		// Exit 
		return Error("-VideoBufferScaler::Resize() | Scaling failed\n");
	}
	
	//Free ctx
	sws_freeContext(resizeCtx);

	//Done
	return 1;
}