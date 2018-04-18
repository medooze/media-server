#include "log.h"
#include "logo.h"
#include <stdlib.h>
extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

Logo::Logo()
{
	//No logo
	frame = NULL;
	frameRGBA = NULL;
	width = 0;
	height = 0;
}

/***********************
* Logo
*	Destructor
************************/
Logo::~Logo()
{
	//Close logo
	Close();
}
int Logo::Close()
{
	//If we have open a gile
	if(frame)
		//Free it
		free(frame);
	//If we have open a gile
	if(frameRGBA)
		//Free it
		free(frameRGBA);
	//No logo
	frame = NULL;
	frameRGBA = NULL;
	width = 0;
	height = 0;
}

int Logo::Load(const char* fileName)
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
	int numpixels = 0;
	int size = 0;

	//Create context from file
	if(avformat_open_input(&fctx, fileName, NULL, NULL)<0)
		return Error("Couldn't open the logo image file [%s]\n",fileName);

	//Check it's ok
	if(avformat_find_stream_info(fctx,NULL)<0)
	{
		//Set error
		res = Error("Couldn't find stream information for the logo image file...\n");
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
	//Only one thread
	ctx->thread_count	= 1;
	
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

	//Use only one thread to avoid decoding on background and logo not displayed
	ctx->thread_count = 1;

	//Decode logo
	if (avcodec_decode_video2(ctx, logoRGB, &gotLogo, &packet)<0)
	{
		//Set errror
		res = Error("Couldn't decode logo\n");
		//Free resources
		av_free_packet(&packet);
		goto end;
	}

	av_free_packet(&packet);

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

	//Get frame sizes
	width = ctx->width;
	height = ctx->height;

	// Create YUV rescaller cotext
	if (!(sws = sws_alloc_context()))
	{
		//Set errror
		res = Error("Couldn't alloc sws context\n");
		// Exit
		goto end;
	}

	// Set property's of YUV rescaller context
	av_opt_set_defaults(sws);
	av_opt_set_int(sws, "srcw",       width			,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "srch",       height		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "src_format", ctx->pix_fmt		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "dstw",       width			,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "dsth",       height		,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "dst_format", AV_PIX_FMT_YUV420P	,AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(sws, "sws_flags",  SWS_FAST_BILINEAR	,AV_OPT_SEARCH_CHILDREN);
	
	// Init YUV rescaller context
	if (sws_init_context(sws, NULL, NULL) < 0)
	{
		//Set errror
		res = Error("Couldn't init sws context\n");
		// Exit
		goto end;
	}

	//Check if we already had one
	if (frame)
		//Free memory
		free(frame);
	//Check if we already had one
	if (frameRGBA)
		//Free memory
		free(frameRGBA);

	//Get size with padding
	size = (((width/32+1)*32)*((height/32+1)*32)*3)/2+AV_INPUT_BUFFER_PADDING_SIZE+32;

	//And numer of pixels
	numpixels = width*height;

	//Allocate frame
	frame = (BYTE*)malloc32(size); /* size for YUV 420 */
	frameRGBA = (BYTE*)malloc32(numpixels*4);

	//Alloc data
	logo->data[0] = frame;
	logo->data[1] = logo->data[0] + numpixels;
	logo->data[2] = logo->data[1] + numpixels / 4;

	//Set size for planes
	logo->linesize[0] = width;
	logo->linesize[1] = width/2;
	logo->linesize[2] = width/2;

	//Convert
	sws_scale(sws, logoRGB->data, logoRGB->linesize, 0, height, logo->data, logo->linesize);

	//Copy logo from rgbA to rgb
	for (int j=0;j<height;j++)
		for (int i=0;i<width;i++)
			//Copy line by line
			memcpy(frameRGBA+(width*j+i)*4,logoRGB->data[0]+logoRGB->linesize[0]*j+i*3,3);
	
	//Everything was ok
	res = 1;

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

int Logo::GetWidth() const
{
	return width;
}

int Logo::GetHeight() const
{
	return height;
}

BYTE* Logo::GetFrame() const
{
	return frame;
}

BYTE* Logo::GetFrameRGBA() const
{
	return frameRGBA;
}
