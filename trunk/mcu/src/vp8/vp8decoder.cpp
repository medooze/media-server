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
	completeFrame = true;
	first = true;
	//Set flags
	vpx_codec_flags_t flags = 0;
	/**< Postprocess decoded frame */
	flags |= VPX_CODEC_USE_POSTPROC;
	
	//Get codec capatbilities
	vpx_codec_caps_t caps = vpx_codec_get_caps(interface);

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
	if(vpx_codec_dec_init(&decoder, interface, NULL, flags)!=VPX_CODEC_OK)
		Error("Error initing VP8 decoder [error %d:%s]\n",decoder.err,decoder.err_detail);

	vp8_postproc_cfg_t  ppcfg;
	 /**< the types of post processing to be done, should be combination of "vp8_postproc_level" */
	ppcfg.post_proc_flag = VP8_DEMACROBLOCK | VP8_DEBLOCK;
	/**< the strength of deblocking, valid range [0, 16] */
	ppcfg.deblocking_level = 3;
	//Set deblocking  settings
	vpx_codec_control(&decoder, VP8_SET_POSTPROC, &ppcfg);
}

/***********************
* ~VP8Decoder
*	Destructor
************************/
VP8Decoder::~VP8Decoder()
{
	vpx_codec_destroy(&decoder);
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
	vpx_codec_err_t err = VPX_CODEC_OK;

	//Not key frame
	isKeyFrame = false;

	//Check if lost
	if (lost)
		//Not complete frame
		completeFrame = false;

	//Check if not empty last packet
	if (inLen && completeFrame)
	{
		VP8PayloadDescriptor desc;

		//Decode payload descriptr
		DWORD pos = desc.Parse(in,inLen);

		//Check if we have been able to parse frame
		if (!pos)
		{
			//Frame not complete
			completeFrame = false;
			//Error
			return Error("Could not parse VP8 payload descriptor");
		}

		//If it is first packetand it is not a partition start
		if (first && !desc.startOfPartition)
		{
			//Frame not complete
			completeFrame = false;
			//Error but continue to handle last mark
			Error("First packet lost of VP8 frame");
		}

		//Next is not first frame
		first = false;

		//If it is first of the partition
		if (desc.startOfPartition && bufLen )
		{
			//Debug("-Decoding partition %d %d\n",desc.partitionIndex-1,bufLen);
			//Discard not compelete partitions
			if (completeFrame)
				//Decode previous partition
				err = vpx_codec_decode(&decoder,buffer,bufLen,NULL,0);
			
			//Reset length
			bufLen = 0;

			//Check error
			if (err!=VPX_CODEC_OK)
				//Error
				Error("Error decoding VP8 partition [error %d:%s]\n",decoder.err,decoder.err_detail);
		}

		//Check size
		if (bufLen+inLen-pos<=bufSize)
		{
			//Append
			memcpy(buffer+bufLen,in+pos,inLen-pos);
			//Increase size
			bufLen+=inLen-pos;
		} else {
			//Frame not complete
			completeFrame = false;
			//Error
			Error("too much data in vp8 partition\n");
		}
	}

	//Si es el ultimo
	if (last)
	{
		//Next is first again
		first = true;

		//Check if it is not compete
		int corrupted = !completeFrame;
		//Get len
		DWORD len = bufLen;

		//Clean next frame
		completeFrame = true;
		
		//Debug("-Decoding last partition %d %d %d\n",inLen,len,bufLen);
		
		//Resetamos el buffer
		bufLen=0;

		//Check it is corrupted
		if (corrupted)
			//Do nothing
			return Error("-Not complete VP8 frame\n");

		//If got last partition
		if (len)
			//Decode last partition
			err = vpx_codec_decode(&decoder,buffer,len,NULL,0);
		/*
		//Check error
		if (err!=VPX_CODEC_OK)
			//Error
			return Error("Error decoding VP8 last [error %d:%s]\n",decoder.err,decoder.err_detail);

		//Decode end of frame
		err = vpx_codec_decode(&decoder,NULL,0,NULL,0);
		*/
		
		//Check error
		if (err!=VPX_CODEC_OK)
			//Error
			return Error("Error decoding VP8 empty [error %d:%s]\n",decoder.err,decoder.err_detail);

		//Check if it is corrupted even if completed
		if (vpx_codec_control(&decoder, VP8D_GET_FRAME_CORRUPTED, &corrupted)==VPX_CODEC_OK)
			//Set key frame flag
			isKeyFrame =  !(buffer[0] & 1) && !corrupted;

		//Check it is corrupted
		if (corrupted)
			//Do nothing
			return Error("-Corrupted VP8 frame\n");
		
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
		int u = width*height;
		int v = width*height*5/4;
		int size = width*height*3/2;

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

bool VP8Decoder::IsKeyFrame()
{
	return isKeyFrame;
}
