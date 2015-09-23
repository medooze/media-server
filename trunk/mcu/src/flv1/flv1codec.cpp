#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "flv1codec.h"
#include "video.h"
#include "av_codec_encode_video.h"


//////////////////////////////////////////////////////////////////////////
//Encoder
// 	Codificador HFLV1
//
//////////////////////////////////////////////////////////////////////////
/***********************
* FLV1Encoder
*	Constructor de la clase
************************/
FLV1Encoder::FLV1Encoder(const Properties& properties)
{
	// Set default values
	frame   = NULL;
	codec   = NULL;
	bufSize = 0;
	type    = VideoCodec::SORENSON;
	qMin	= 1;
	qMax	= 31;
	format  = 0;

	// Init framerate
	SetFrameRate(5,300,20);

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_FLV1);

	// Check codec
	if(!codec)
		Error("No codec found\n");

	//No estamos abiertos
	opened = false;
 
	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);
	picture = av_frame_alloc();
}

/***********************
* ~FLV1Encoder
*	Destructor
************************/
FLV1Encoder::~FLV1Encoder()
{
	if (frame);
		delete(frame);

	if (ctx)
	{
		avcodec_close(ctx);
		free(ctx);
	}

	if (picture)
		free(picture);
}

/***********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
************************/
int FLV1Encoder::SetSize(int width, int height)
{
	Log("-SetSize [%d,%d]\n",width,height);

	// Set pixel format 
	ctx->pix_fmt		= AV_PIX_FMT_YUV420P;
	ctx->width 		= width;
	ctx->height 		= height;

	// Set picture data
	picture->linesize[0] = width;
	picture->linesize[1] = width/2;
	picture->linesize[2] = width/2;

	// Open codec
	return OpenCodec();
}

/*************************
* SetFrameRate
* 	Indica el numero de frames por segudno actual
***************************/
int FLV1Encoder::SetFrameRate(int frames,int kbits,int intraPeriod)
{
	// Save frame rate
	if (frames>0)
		fps=frames;
	else
		fps=10;

	// Save bitrate
	if (kbits>0)
		bitrate=kbits*1024;

	//Save intra period
	if (intraPeriod>0)
		this->intraPeriod = intraPeriod;

	return 1;
}

/***********************
* OpenCodec
*	Abre el codec
************************/
int FLV1Encoder::OpenCodec()
{
	Log("-OpenCodec FLV1 [%dbps,%dfps]\n",bitrate,fps);

	// Check
	if (codec==NULL)
		return Error("No codec\n");

	// Check 
	if (opened)
		return Error("Already opened\n");

	//If already got a buffer
	if (frame)
		//Free it
		delete(frame);

	//Set new buffer size
	bufSize = (int)bitrate/8;

	//Check size
	if (bufSize<65535)
		//Set minimun
		bufSize = 65535;

	//Y alocamos el buffer
	frame = new VideoFrame(type,bufSize);

	// Bitrate,fps
	ctx->bit_rate 		= bitrate;
	ctx->bit_rate_tolerance = (int)bitrate/fps+1;
	ctx->time_base          = (AVRational){1,fps};
	ctx->gop_size		= intraPeriod;

	// Encoder quality
	//ctx->rc_eq		= "tex^qComp";
	//ctx->rc_min_rate 	= bitrate;
	//ctx->rc_max_rate	= bitrate;
	//ctx->rc_buffer_size	= bitrate;
	//ctx->rc_qsquish 	= 1;
	ctx->max_b_frames	= 0;
	ctx->me_method		= ME_EPZS;
	ctx->mb_decision	= FF_MB_DECISION_RD;

	// Flags
	ctx->flags |= CODEC_FLAG_PASS1;
	ctx->flags |= CODEC_FLAG_MV0;

	//Set input bits
	ctx->bits_per_raw_sample = 8;
	
	// Open codec
	if (avcodec_open2(ctx, codec, NULL)<0)
		return Error("Unable to open FLV1 codec\n");

	// We are opened
	opened=true;

	// Exit
	return 1;
}


/***********************
* EncodeFrame
*	Codifica un frame
************************/
VideoFrame* FLV1Encoder::EncodeFrame(BYTE *in,DWORD len)
{
	//Check if we are opened
	if (!opened)
	{
		Error("FLV1 encoder not opened");
		return NULL;
	}

	int numPixels = ctx->width*ctx->height;

	//Comprobamos el tama�o
	if (numPixels*3/2 != len)
	{
		Error("-EncodeFrame length error [%d,%d]\n",numPixels*5/4,len);
		return NULL;
	}

	//POnemos los valores
	picture->data[0] = in;
	picture->data[1] = in+numPixels;
	picture->data[2] = in+numPixels*5/4;

	//Codificamos
	bufLen=avcodec_encode_video(ctx,frame->GetData(),frame->GetMaxMediaLength(),picture);

	//Set length
	frame->SetLength(bufLen);

	//Set width and height
	frame->SetWidth(ctx->width);
	frame->SetHeight(ctx->height);

	//Is intra
	frame->SetIntra(ctx->coded_frame->key_frame);

	//Unset fpu
	picture->key_frame = 0;
	picture->pict_type = AV_PICTURE_TYPE_NONE;

	//Y ponemos a cero el comienzo
	bufIni=0;

	return frame;
}

/***********************
* FastPictureUpdate
*	Manda un frame entero
************************/
int FLV1Encoder::FastPictureUpdate()
{
	Log("-FLV1Encoder FastPictureUpdate\n");
	//If we have picture
	if (picture)
	{
		//Set it
		picture->key_frame = 1;
		picture->pict_type = AV_PICTURE_TYPE_I;
	}
	return 1;
}

//////////////////////////////////////////////////////////////////////////
//FLV1Decoder
// 	Decodificador FLV1
//
//////////////////////////////////////////////////////////////////////////
/***********************
* FLV1Decoder
*	Consturctor
************************/
FLV1Decoder::FLV1Decoder()
{
	//Guardamos los valores por defecto
	codec = NULL;
	type = VideoCodec::SORENSON;
	bufLen = 0;
	
	//Encotramos el codec
	codec = avcodec_find_decoder(AV_CODEC_ID_FLV1);

	//Comprobamos
	if(codec==NULL)
	{
		Error("No decoder found\n");
		return ;
	}

	//Alocamos el contxto y el picture
	ctx = avcodec_alloc_context3(codec);
	picture = av_frame_alloc();

	//POnemos los valores del contexto
	ctx->workaround_bugs 	= 255*255;
	ctx->error_concealment 	= FF_EC_GUESS_MVS | FF_EC_DEBLOCK;

	//Alocamos el buffer
	frame = NULL;
	frameSize = 0;
	src = 0;
	
	//Lo abrimos
	avcodec_open2(ctx, codec, NULL);
}

/***********************
* ~FLV1Decoder
*	Destructor
************************/
FLV1Decoder::~FLV1Decoder()
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
int FLV1Decoder::DecodePacket(BYTE *buffer,DWORD bufferLen,int lost,int last)
{
	return 0;
}

int FLV1Decoder::Decode(BYTE *buffer,DWORD size)
{
	//Decodificamos	
	int got_picture=0;

	//Decodificamos
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = buffer;
	pkt.size = size;

	if (avcodec_decode_video2(ctx, picture, &got_picture, &pkt)<0)
		return 0;

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
