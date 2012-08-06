#include <framescaler.h>
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

FrameScaler::FrameScaler()
{
	// No resize 
	resizeCtx	= NULL;
	resizeWidth	= 0;
	resizeHeight	= 0;
	resizeDstWidth	= 0;
	resizeDstHeight = 0;
	resizeDstLineWidth = 0;

	//tmp buffer
	tmpWidth  	= 0;
	tmpHeight 	= 0;
	tmpBuffer	= NULL;
	tmpBufferSize	= 0;
	tmpY		= NULL;
	tmpU		= NULL;
	tmpV		= NULL;

	// Bicubic by default 
	resizeFlags	= SWS_BICUBIC;
}
FrameScaler::~FrameScaler()
{
	//Free mem
	if (tmpBuffer)
		//free
		free(tmpBuffer);
	//If we got a context
	if (resizeCtx)
		//Closse context
		sws_freeContext(resizeCtx);
}

int FrameScaler::SetResize(int srcWidth,int srcHeight,int srcLineWidth,int dstWidth,int dstHeight,int dstLineWidth)
{
	// Check Size
	if (!srcWidth || !srcHeight || !srcLineWidth || !dstWidth || !dstHeight || !dstLineWidth)
	{
		//If we got a context
		if (resizeCtx)
			//Closse context
			sws_freeContext(resizeCtx);
		// No valid context
		resizeCtx = NULL;
		//Exit
		return 0;
	}

	// Check if we already have a scaler for this
	if (resizeCtx && (resizeWidth==srcWidth) && (srcHeight==resizeHeight) && (dstWidth==resizeDstWidth) && (dstHeight==resizeDstHeight))
		//Done
		return 1;

	//If we already got a context
	if (resizeCtx)
		//Closse context
		sws_freeContext(resizeCtx);

	// Create new context
	if (!(resizeCtx = sws_alloc_context()))
		// Exit 
		return 0;

	// Set property's of context
	av_opt_set_defaults(resizeCtx);
	av_opt_set_int(resizeCtx, "srcw",       srcWidth		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "srch",       srcHeight		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "src_format", PIX_FMT_YUV420P		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "dstw",       dstWidth		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "dsth",       dstHeight		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "dst_format", PIX_FMT_YUV420P		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "sws_flags",  resizeFlags		,AV_OPT_SEARCH_CHILDREN);
	
	// Init context
	if (sws_init_context(resizeCtx, NULL, NULL) < 0)
	{
		//Free context
		sws_freeContext(resizeCtx);
		//Nullify it
		resizeCtx = NULL;
		// Exit 
		return Error("Couldn't init sws context");
	}

	// Set values 
	resizeWidth 	= srcWidth;
	resizeHeight 	= srcHeight;
	resizeDstWidth	= dstWidth;
	resizeDstHeight = dstHeight;
	resizeDstLineWidth = dstLineWidth;

	//to use MM2 we need the width and heinght to be multiple of 32
	tmpWidth = (resizeDstWidth/32 +1)*32;
	tmpHeight = (resizeDstHeight/32 +1)*32;

	//Get tmp buffer size
	tmpBufferSize = tmpWidth*tmpHeight*3/2+FF_INPUT_BUFFER_PADDING_SIZE+32;

	//Check if we had it already
	if (tmpBuffer)
		//Free it
		free(tmpBuffer);

	//Allocate it
	tmpBuffer = (BYTE*)malloc(tmpBufferSize);

	// Set values for line sizes
	resizeSrc[0] = srcLineWidth;
	resizeSrc[1] = srcLineWidth/2;
	resizeSrc[2] = srcLineWidth/2;
	/*resizeDst[0] = dstLineWidth;
	resizeDst[1] = dstLineWidth/2;
	resizeDst[2] = dstLineWidth/2;*/
	resizeDst[0] = tmpWidth;
	resizeDst[1] = tmpWidth/2;
	resizeDst[2] = tmpWidth/2;

	//Get tmp planes
	tmpY = ALIGNTO32(tmpBuffer);
	tmpU = tmpY+tmpWidth*tmpHeight;
	tmpV = tmpU+tmpWidth*tmpHeight/4;

	// exit 
	return 1;
}

int FrameScaler::Resize(BYTE *srcY,BYTE *srcU,BYTE *srcV,BYTE *dstY, BYTE *dstU, BYTE *dstV)
{
	// src & dst 
	BYTE* src[3];
	BYTE* dst[3];

	// Check 
	if (!resizeCtx)
		//Error
		return 0;

	// Set pointers 
	src[0] = srcY;
	src[1] = srcU;
	src[2] = srcV;
	/*dst[0] = dstY;
	dst[1] = dstU;
	dst[2] = dstV;*/
	dst[0] = tmpY;
	dst[1] = tmpU;
	dst[2] = tmpV;

	// Resize frame 
	sws_scale(resizeCtx, src, resizeSrc, 0, resizeHeight, dst, resizeDst);

	//Copy to destination
	for (int i=0;i<resizeDstHeight;++i)
		//Copy
		memcpy(dstY+resizeDstLineWidth*i,tmpY+tmpWidth*i,resizeDstWidth);

	//Copy to destination
	for (int i=0;i<resizeDstHeight/2;++i)
	{
		//Copy
		memcpy(dstU+resizeDstLineWidth*i/2,tmpU+tmpWidth*i/2,resizeDstWidth/2);
		memcpy(dstV+resizeDstLineWidth*i/2,tmpV+tmpWidth*i/2,resizeDstWidth/2);
	}

	//Done
	return 1;
} 

int FrameScaler::Resize(BYTE *src,DWORD srcWidth,DWORD srcHeight,BYTE *dst,DWORD dstWidth,DWORD dstHeight)
{
	//If sizes are the inot same
	if (srcWidth!=dstWidth ||srcHeight!=dstHeight)
	{
		//Set resize with full image
		SetResize(srcWidth,srcHeight,srcWidth,dstWidth,dstHeight,dstWidth);
		//Calc pointers
		DWORD srcPixels = srcWidth*srcHeight;
		BYTE *srcY = src;
		BYTE *srcU = src+srcPixels;
		BYTE *srcV = src+srcPixels*5/4;
		DWORD dstPixels = dstWidth*dstHeight;
		BYTE *dstY = dst;
		BYTE *dstU = dst+dstPixels;
		BYTE *dstV = dst+dstPixels*5/4;

		//Resize imgae
		Resize(srcY,srcU,srcV,dstY,dstU,dstV);
	} else {
		//Just copy
		memcpy(dst,src,srcWidth*srcHeight*3/2);
	}

	return 1;
}
