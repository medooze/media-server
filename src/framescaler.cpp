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
	resizeDstAdjustedHeight = 0;
	resizeDstAdjustedWidth  = 0;
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

int FrameScaler::SetResize(int srcWidth,int srcHeight,int srcLineWidth,int dstWidth,int dstHeight,int dstLineWidth,bool keepAspectRatio)
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

	// Set values
	resizeWidth		= srcWidth;
	resizeHeight		= srcHeight;
	resizeDstWidth		= dstWidth;
	resizeDstHeight		= dstHeight;
	resizeDstLineWidth	= dstLineWidth;
	resizeDstAdjustedWidth  = dstWidth;
	resizeDstAdjustedHeight = dstHeight;

	//Check aspect ratio flag
	if (keepAspectRatio)
	{
		//Get ratios
		double srcRatio = (double)srcWidth/srcHeight;
		double dstRatio = (double)dstWidth/dstHeight;
		//Compare ratios
		if (srcRatio<dstRatio)
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
			resizeDstAdjustedWidth  = dstHeight*srcRatio;
			resizeDstAdjustedHeight = dstHeight;
		} else if (srcRatio>dstRatio) {
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
			resizeDstAdjustedWidth  = dstWidth;
			resizeDstAdjustedHeight = dstWidth/srcRatio;
		}
	}
	
	// Set property's of context
	av_opt_set_defaults(resizeCtx);
	av_opt_set_int(resizeCtx, "srcw",       srcWidth		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "srch",       srcHeight		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "src_format", AV_PIX_FMT_YUV420P	,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "dstw",       resizeDstAdjustedWidth	,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "dsth",       resizeDstAdjustedHeight	,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(resizeCtx, "dst_format", AV_PIX_FMT_YUV420P	,AV_OPT_SEARCH_CHILDREN);
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

	//to use MM2 we need the width and heinght to be multiple of 32
	tmpWidth = (resizeDstAdjustedWidth/32 +1)*32;
	tmpHeight = (resizeDstAdjustedHeight/32 +1)*32;

	//Get tmp buffer size
	tmpBufferSize = tmpWidth*tmpHeight*3/2+AV_INPUT_BUFFER_PADDING_SIZE+32;

	//Check if we had it already
	if (tmpBuffer)
		//Free it
		free(tmpBuffer);

	//Allocate it
	tmpBuffer = (BYTE*)malloc32(tmpBufferSize);

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

	//Get offsets due to vertical lines (mast be even)
	DWORD x = (resizeDstWidth-resizeDstAdjustedWidth)/2 & ~1;
	DWORD y = (resizeDstHeight-resizeDstAdjustedHeight)/2 & ~1;

	//Copy to destination
	for (DWORD i=0;i<resizeDstHeight;++i)
	{
		//Check for horizontal bars
		if ((i<y) || (i>=(resizeDstAdjustedHeight+y)))
		{
			//Fill with black
			memset(dstY+resizeDstLineWidth*i,0,resizeDstWidth);
		} else {
			//Check for vertical bars
			if (x>0)
			{
				//Set them
				memset(dstY+resizeDstLineWidth*i,0,x);
				memset(dstY+resizeDstLineWidth*i+x+resizeDstAdjustedWidth,0,resizeDstWidth-resizeDstAdjustedWidth-x);
			}
			//Copy image in the middle
			memcpy(dstY+resizeDstLineWidth*i+x,tmpY+tmpWidth*(i-y),resizeDstAdjustedWidth);
		}
	}

	//Half them
	x = x/2;
	y = y/2;

	//Copy to destination
	for (int i=0;i<resizeDstHeight/2;++i)
	{
		//Check for horizontal bars
		if ((i<y) || (i>=(resizeDstAdjustedHeight/2+y)))
		{
			//Fill with black
			memset(dstU+resizeDstLineWidth*i/2,(BYTE)-128,resizeDstWidth/2);
			memset(dstV+resizeDstLineWidth*i/2,(BYTE)-128,resizeDstWidth/2);
		} else {
			//Check for vertical bars
			if (x>0)
			{
				//Set them
				memset(dstU+resizeDstLineWidth*i/2,(BYTE)-128,x);
				memset(dstV+resizeDstLineWidth*i/2,(BYTE)-128,x);
				memset(dstU+resizeDstLineWidth*i/2+x+resizeDstAdjustedWidth/2,(BYTE)-128,resizeDstWidth/2-resizeDstAdjustedWidth/2-x);
				memset(dstV+resizeDstLineWidth*i/2+x+resizeDstAdjustedWidth/2,(BYTE)-128,resizeDstWidth/2-resizeDstAdjustedWidth/2-x);
			}
			//Copy
			memcpy(dstU+resizeDstLineWidth*i/2+x,tmpU+tmpWidth*(i-y)/2,resizeDstAdjustedWidth/2);
			memcpy(dstV+resizeDstLineWidth*i/2+x,tmpV+tmpWidth*(i-y)/2,resizeDstAdjustedWidth/2);
		}
	}

	//Done
	return 1;
} 

int FrameScaler::Resize(BYTE *src,DWORD srcWidth,DWORD srcHeight,BYTE *dst,DWORD dstWidth,DWORD dstHeight, bool keepAspectRatio)
{
	//If sizes are the inot same
	if (srcWidth!=dstWidth ||srcHeight!=dstHeight)
	{
		//Set resize with full image
		SetResize(srcWidth,srcHeight,srcWidth,dstWidth,dstHeight,dstWidth,keepAspectRatio);
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
