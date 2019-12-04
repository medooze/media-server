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
VP9Decoder::VP9Decoder()
{
	type = VideoCodec::VP9;
	frame = NULL;
	frameSize = 0;
	width = 0;
	height = 0;
	completeFrame = true;
	first = true;
	
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
	if (frame)
		free(frame);
}

/***********************
* DecodePacket
*	Decodifica un packete
************************/
int VP9Decoder::DecodePacket(const BYTE *data,DWORD size,int lost,int last)
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
			ret = Decode(frame->GetData(),frame->GetLength());
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

int VP9Decoder::Decode(const BYTE *buffer,DWORD len)
{
	//Decode
	vpx_codec_err_t err = vpx_codec_decode(&decoder,buffer,len,NULL,0);
	
	//Check error
	if (err!=VPX_CODEC_OK)
		//Error
		return Error("Error decoding VP9 frame [error %d:%s]\n",decoder.err,decoder.err_detail);

	//Ger image
	vpx_codec_iter_t iter = NULL;
	vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);

	//Check img
	if (!img)
		//Do nothing
		return Error("-No image\n");

	//Get dimensions
	width = img->d_w;
	height = img->d_h;
	DWORD u = width*height;
	DWORD v = width*height*5/4;
	DWORD size = width*height*3/2;

	//Check size
	if (size>frameSize)
	{
		Log("-Frame size %dx%d\n",width,height);
		//Liberamos si habia
		if(frame!=NULL)
			free(frame);
		//Y allocamos de nuevo
		frame = (BYTE*) malloc(size);
		frameSize = size;
	}

	//Copaamos  el Cy
	for(DWORD i=0;i<height;i++)
		memcpy(&frame[i*width],&img->planes[0][i*img->stride[0]],width);

	//Y el Cr y Cb
	for(DWORD i=0;i<height/2;i++)
	{
		memcpy(&frame[i*width/2+u],&img->planes[1][i*img->stride[1]],width/2);
		memcpy(&frame[i*width/2+v],&img->planes[2][i*img->stride[2]],width/2);
	}

	//Return ok
	return 1;
}

bool VP9Decoder::IsKeyFrame()
{
	return isKeyFrame;
}
