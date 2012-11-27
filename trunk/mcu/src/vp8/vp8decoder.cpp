/* 
 * File:   vp8decoder.cpp
 * Author: Sergio
 * 
 * Created on 13 de noviembre de 2012, 15:57
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vp8decoder.h"
#include "vp8.h"
#include "log.h"

#define interface (vpx_codec_vp8_dx())
/***********************
* VP8Decoder
*	Consturctor
************************/
VP8Decoder::VP8Decoder()
{
	type = VideoCodec::VP8;

	//Guardamos los valores por defecto
	buffer = NULL;
	bufLen = 0;
	bufSize = 0;

	//Alocamos el buffer
	bufSize = 1024*756*3/2;
	buffer = (BYTE *)malloc(bufSize);
	frame = NULL;
	frameSize = 0;
	src = 0;
	width = 0;
	height = 0;
	//Set flags
	vpx_codec_flags_t flags = 0;
	flags |= VPX_CODEC_USE_POSTPROC;

	//Init decoder
	if(vpx_codec_dec_init(&decoder, interface, NULL, flags)!=VPX_CODEC_OK)
		Error("Error initing VP8 decoder [error %d:%s]\n",decoder.err,decoder.err_detail);

	Log("decoder %p\n",decoder.iface);
}

/***********************
* ~VP8Decoder
*	Destructor
************************/
VP8Decoder::~VP8Decoder()
{
	if (buffer)
		free(buffer);
	if (frame)
		free(frame);
}

/***********************
* DecodePacket
*	Decodifica un packete
************************/
int VP8Decoder::DecodePacket(BYTE *in,DWORD inLen,int lost,int last)
{
	vpx_codec_err_t err;
	VP8PayloadDescriptor desc;

	//Decode payload descriptr
	DWORD pos = desc.Parse(in,inLen);

	//Check
	if (!pos)
		//Error
		return Error("Could not parse VP8 payload descriptor");

	//If it is first of the partition
	if ((desc.startOfPartition || lost) && bufLen)
	{
		//Decode previous partition
		err = vpx_codec_decode(&decoder,buffer,bufLen,NULL,0);

		//Reset length
		bufLen = 0;

		//Check error
		if (err!=VPX_CODEC_OK)
			//Error
			return Error("Error decoding VP8 partition [error %d:%s]\n",decoder.err,decoder.err_detail);

	}

	//Check size
	if (bufLen+inLen-pos>bufSize)
	{
		//Reset
		bufLen = 0;
		//Error
		return Error("too much data in vp8 partition\n");
	}
	
	//Append
	memcpy(buffer+bufLen,in+pos,inLen-pos);

	//Increase size
	bufLen+=inLen-pos;

	//Si es el ultimo
	if(last)
	{
		//Decode last partition
		err = vpx_codec_decode(&decoder,buffer,bufLen,NULL,0);

		//Y resetamos el buffer
		bufLen=0;

		//Check error
		if (err!=VPX_CODEC_OK)
			//Error but do not exit yet, try to get a frame anyway
			 Error("Error decoding VP8 last partition [error %d:%s]\n",decoder.err,decoder.err_detail);
		
		//Ger image
		vpx_codec_iter_t iter = NULL;
		vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);
		//Check img
		if (!img)
			return Error("No frame available\n");
		//Get dimensions
		width = img->d_w;
		height = img->d_h;
		int u = width*height;
		int v = width*height*5/4;
		int size = width*height*3/2;

		//Comprobamos el tama�o
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
		for(int i=0;i<height;i++)
			memcpy(&frame[i*width],&img->planes[0][i*img->stride[0]],width);

		//Y el Cr y Cb
		for(int i=0;i<height/2;i++)
		{
			memcpy(&frame[i*width/2+u],&img->planes[1][i*img->stride[1]],width/2);
			memcpy(&frame[i*width/2+v],&img->planes[2][i*img->stride[2]],width/2);
		}
	}

	//Return ok
	return 1;
}

int VP8Decoder::Decode(BYTE *buffer,DWORD size)
{
	return 0;
}

