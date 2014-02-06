#include "overlay.h"
#include "log.h"
#include "bitstream.h"
#include <stdlib.h>
#include <string.h>
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
}

Overlay::Overlay(DWORD width,DWORD height)
{
	//Store values
	this->width = width;
	this->height = height;
	//Calculate size for overlay iage with alpha
	overlaySize = width*height*5/2+FF_INPUT_BUFFER_PADDING_SIZE+32;
	//Create overlay image
	overlayBuffer = (BYTE*)malloc(overlaySize);
	//Get aligned buffer
	overlay = ALIGNTO32(overlayBuffer);
	//Calculate size for final image i.e. without alpha
	imageSize = width*height*3/2+FF_INPUT_BUFFER_PADDING_SIZE+32;
	//Create final image
	imageBuffer = (BYTE*)malloc(imageSize);
	//Get aligned buffer
	image = ALIGNTO32(imageBuffer);
	//Do not display
	display = false;
}

Overlay::~Overlay()
{
	//Free memor
	free(overlayBuffer);
	free(imageBuffer);
}

int Overlay::LoadPNG(const char* filename)
{
	AVFormatContext *fctx = NULL;
	AVCodecContext *ctx = NULL;
	AVCodec *codec = NULL;
	AVFrame *logoRGB = NULL;
	AVFrame* logo = NULL;
	SwsContext *sws = NULL;
	AVPacket packet;
	int res = 0;
	int gotLogo = 0;
	int numpixels = width*height;

	//Init ffmpeg in case it wasn't
	av_register_all();

	//Create context from file
	if(avformat_open_input(&fctx, filename, NULL, NULL)<0)
		//Exit
		return Error("Couldn't open the png image file [%s]\n",filename);

	//Check it's ok
	if(avformat_find_stream_info(fctx,NULL)<0)
	{
		//Set error
		res = Error("Couldn't find stream information for the png image file...\n");
		//Free resources
		goto end;
	}

	//Get codec from file fromat
	if (!(ctx = fctx->streams[0]->codec))
	{
		//Set errror
		res = Error("Context codec not valid\n");
		//Free resources
		goto end;
	}

	//Get decoder for format
	if (!(codec = avcodec_find_decoder(ctx->codec_id)))
	{
		//Set errror
		res = Error("Couldn't find codec for the logo image file...\n");
		//Free resources
		goto end;
	}

	//Open codec
	if (avcodec_open2(ctx, codec, NULL)<0)
	{
		//Set errror
		res = Error("Couldn't open codec for the logo image file...\n");
		//Free resources
		goto end;
	}

	//Read logo frame
	if (av_read_frame(fctx, &packet)<0)
	{
		//Set errror
		res = Error("Couldn't read frame from the image file...\n");
		//Free resources
		goto end;
	}

	//Alloc frame
	if (!(logoRGB = av_frame_alloc()))
	{
		//Set errror
		res = Error("Couldn't alloc frame\n");
		//Free resources
		goto end;
	}

	//Decode logo
	if (avcodec_decode_video2(ctx, logoRGB, &gotLogo, &packet)<0)
	{
		//Set errror
		res = Error("Couldn't decode logo\n");
		//Free resources
		goto end;
	}

	//If it we don't have a logo
	if (!gotLogo)
	{
		//Set errror
		res = Error("No logo on file\n");
		//Free resources
		goto end;
	}

	//Allocate new one
	if (!(logo = av_frame_alloc()))
	{
		//Set errror
		res = Error("Couldn't alloc frame\n");
		//Free resources
		goto end;
	}

	// Create YUV rescaller cotext
	if (!(sws = sws_alloc_context()))
	{
		//Set errror
		res = Error("Couldn't alloc sws context\n");
		// Exit
		goto end;
	}

	//First conver to RGBA
	av_opt_set_defaults(sws);
	av_opt_set_int(sws, "srcw",       ctx->width		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "srch",       ctx->height		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "src_format", ctx->pix_fmt		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "dstw",       width			,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "dsth",       height		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "dst_format", PIX_FMT_YUVA420P	,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "sws_flags",  SWS_FAST_BILINEAR	,AV_OPT_SEARCH_CHILDREN);

	// Init YUV rescaller context
	if (sws_init_context(sws, NULL, NULL) < 0)
	{
		//Set errror
		res = Error("Couldn't init sws context\n");
		// Exit
		goto end;
	}

	//Set size for planes
	logo->linesize[0] = width;
	logo->linesize[1] = width/2;
	logo->linesize[2] = width/2;
	logo->linesize[3] = width;

	//Alloc data
	logo->data[0] = overlay;
	logo->data[1] = logo->data[0] + numpixels;
	logo->data[2] = logo->data[1] + numpixels / 4;
	logo->data[3] = logo->data[2] + numpixels / 4;

	//Convert
	sws_scale(sws, logoRGB->data, logoRGB->linesize, 0, height, logo->data, logo->linesize);

	//Everything was ok
	res = 1;
	
	//Display it then
	display = true;
end:
	if (logo)
		av_free(logo);

	if (logoRGB)
		av_free(logoRGB);

	if (ctx)
		avcodec_close(ctx);

	if (sws)
		sws_freeContext(sws);

	if (fctx)
		avformat_close_input(&fctx);

	//Exit
	return res;
}

int Overlay::GenerateSVG(const char*)
{

}

BYTE* Overlay::Display(BYTE* frame)
{
	//check if we have overlay
	if (!display)
		//Return the same frame
		return frame;
	//Get source
	BYTE* srcY1 = frame;
	BYTE* srcY2 = frame+width;
	BYTE* srcU  = frame+width*height;
	BYTE* srcV  = frame+width*height*5/4;
	//Get overlay
	BYTE* ovrY1 = overlay;
	BYTE* ovrY2 = overlay+width;
	BYTE* ovrU  = overlay+width*height;
	BYTE* ovrV  = overlay+width*height*5/4;
	BYTE* ovrA1 = overlay+width*height*3/2;
	BYTE* ovrA2 = overlay+width*height*3/2+width;
	//Get destingation
	BYTE* dstY1 = image;
	BYTE* dstY2 = image+width;
	BYTE* dstU  = image+width*height;
	BYTE* dstV  = image+width*height*5/4;

	for (int j=0; j<height/2; ++j)
	{
		for (int i=0; i<width/2; ++i)
		{
			//Get alpha values
			BYTE a11 = *(ovrA1++);
			BYTE a12 = *(ovrA1++);
			BYTE a21 = *(ovrA2++);
			BYTE a22 = *(ovrA2++);
			//Check
			if (a11==0)
			{
				//Set alpha
				*(dstY1++) = *(srcY1++);
				//Increase pointer
				++ovrY1;
			} else if (a11==255) {
				//Set original image
				*(dstY1++) = *(ovrY1++);
				//Increase pointer
				++srcY1;
			} else {
				DWORD n = 255-a11;
				DWORD oY = *(ovrY1++);
				DWORD sY = *(srcY1++);
				*(dstY1++) = (oY*a11+sY*n)/255;
				//Calculate and move pointer
				//*(dstY1++) = (((DWORD)(*(ovrY1++)))*a11 + (((DWORD)(*(srcY1++)))*(~a11)))/255;
			}
			//Check
			if (a12==0)
			{
				//Set alpha
				*(dstY1++) = *(srcY1++);
				//Increase pointer
				++ovrY1;
			} else if (a12==255) {
				//Set original image
				*(dstY1++) = *(ovrY1++);
				//Increase pointer
				++srcY1;
			} else {
				DWORD n = 255-a12;
				DWORD oY = *(ovrY1++);
				DWORD sY = *(srcY1++);
				*(dstY1++) = (oY*a12+sY*n)/255;
				//Calculate and move pointer
				//*(dstY1++) = (((DWORD)(*(ovrY1++)))*a12 + (((DWORD)(*(srcY1++)))*(~a12)))/255;
			}
			//Check
			if (a21==0)
			{
				//Set alpha
				*(dstY2++) = *(srcY2++);
				//Increase pointer
				++ovrY2;
			} else if (a21==255) {
				//Set original image
				*(dstY2++) = *(ovrY2++);
				//Increase pointer
				++srcY2;
			} else {
				DWORD n = 255-a21;
				DWORD oY = *(ovrY2++);
				DWORD sY = *(srcY2++);
				*(dstY2++) = (oY*a21+sY*n)/255;
				//Calculate and move pointer
				//*(dstY2++) = (((DWORD)(*(ovrY2++)))*a21 + (((DWORD)(*(srcY2++)))*(~a21)))/255;
			}
			//Check
			if (a22==0)
			{
				//Set alpha
				*(dstY2++) = *(srcY2++);
				//Increase pointer
				++ovrY2;
			} else if (a22==255) {
				//Set original image
				*(dstY2++) = *(ovrY2++);
				//Increase pointer
				++srcY2;
			} else {
				DWORD n = 255-a22;
				DWORD oY = *(ovrY2++);
				DWORD sY = *(srcY2++);
				*(dstY2++) = (oY*a22+sY*n)/255;
				//Calculate and move pointer
				//*(dstY2++) = (((DWORD)(*(ovrY2++)))*a22 + (((DWORD)(*(srcY2++)))*(~a22)))/255;
			}
			//Summ all alphas
			DWORD alpha = a11+a21+a21+a22;
			//Check UV
			if (alpha==0)
			{
				//Set UV
				*(dstU++) = *(srcU++);
				*(dstV++) = *(srcV++);
				//Increase pointers
				++ovrU;
				++ovrV;
			} else if (alpha == 1020) {
				//Set UV
				*(dstU++) = *(ovrU++);
				*(dstV++) = *(ovrV++);
				//Increase pointers
				++srcU;
				++srcV;
			} else {
				DWORD negalpha = 1020-alpha;
				DWORD oU = *(ovrU++);
				DWORD oV = *(ovrV++);
				DWORD sU = *(srcU++);
				DWORD sV = *(srcV++);
				*(dstU++) = (oU*alpha+sU*negalpha)/1020;
				*(dstV++) = (oV*alpha+sV*negalpha)/1020;
				//Calculate and move pointer
				//*(dstU++) = (((DWORD)(*(ovrU++)))*alpha + (((DWORD)(*(srcU++)))*negalpha))/1020;
				//*(dstV++) = (((DWORD)(*(ovrV++)))*alpha + (((DWORD)(*(srcV++)))*negalpha))/1020;

			}
		}
		//Skip Y line
		srcY1 += width;
		srcY2 += width;
		dstY1 += width;
		dstY2 += width;
		ovrY1 += width;
		ovrY2 += width;
		ovrA1 += width;
		ovrA2 += width;
	}

	//And return overlay
	return image;
}
