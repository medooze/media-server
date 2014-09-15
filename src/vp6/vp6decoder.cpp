#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "vp6decoder.h"
#include "video.h"


/***********************
* VP6Decoder
*	Consturctor
************************/
VP6Decoder::VP6Decoder()
{
	//Guardamos los valores por defecto
	codec = NULL;
	type = VideoCodec::VP6;
	bufLen = 0;

	//Encotramos el codec - note the F flash version
	codec = avcodec_find_decoder(AV_CODEC_ID_VP6F);

	//Comprobamos
	if(codec==NULL)
	{
		Error("vp6: No decoder found\n");
		return ;
	}

	//Alocamos el contxto y el picture
	ctx = avcodec_alloc_context3(codec);
	picture = av_frame_alloc();

	//POnemos los valores del contexto
	ctx->workaround_bugs 	= 255*255;

	//Alocamos el buffer
	frame = NULL;
	frameSize = 0;
	src = 0;
	
	//Lo abrimos
	int ret = avcodec_open2(ctx, codec, NULL);
        if (ret < 0)
        {
                av_free(ctx);
                ctx = NULL;
                Error("VP6: could not open decoder. err=%d\n", ret);
        }
	Log("VP6: decoder ope.\n");
}

/***********************
* ~FLV1Decoder
*	Destructor
************************/
VP6Decoder::~VP6Decoder()
{
	if (frame!=NULL)
		free(frame);
	avcodec_close(ctx);
	free(picture);
	free(ctx);
}

/***********************
* DecodePacket 
*	Decodifica un packete
************************/
int VP6Decoder::DecodePacket(BYTE *buffer,DWORD bufferLen,int lost,int last)
{
	return 0;
}

int VP6Decoder::Decode(BYTE *buffer,DWORD size)
{
	//Decodificamos	
	int got_picture=0, ret;

	//Decodificamos
	AVPacket pkt;
	av_init_packet(&pkt);
	if ( buffer[0]== 0 )
	{
		// Skip unused first byte
		pkt.data = buffer + 1;
	}
	else
	{
		pkt.data = buffer;
	}
	pkt.size = size;

	if (ctx == NULL)
		return Error("vp6: Decode() codec not open.\n");
	ret = avcodec_decode_video2(ctx, picture, &got_picture, &pkt);

	if (ret <0)
	{
		char fferr[200];
		av_strerror(ret, fferr, sizeof(fferr));
		return Error("vp6: decoding error: %s.\n", fferr);
	}
	//Si hay picture
	if (got_picture)
	{
		if(ctx->width==0 || ctx->height==0)
			return 0;

		int w = ctx->width;
		int h = ctx->height;
		int u = w*h;
		int v = w*h*5/4;
		int size = w*h*3/2;

		//Comprobamos el tama�o
		if (size>frameSize)
		{
			//Liberamos si habia
			if(frame!=NULL)
				free(frame);
			//Y allocamos de nuevo
			frame = (BYTE*) malloc(size);
			frameSize = size;
		}
		

		//Copaamos  el Cy
		for(int i=0;i<ctx->height;i++)
			memcpy(&frame[i*w],&picture->data[0][i*picture->linesize[0]],w);

		//Y el Cr y Cb
		for(int i=0;i<ctx->height/2;i++)
		{
			memcpy(&frame[i*w/2+u],&picture->data[1][i*picture->linesize[1]],w/2);
			memcpy(&frame[i*w/2+v],&picture->data[2][i*picture->linesize[2]],w/2);
		}

		return 1;
	}

	return 0;
}
