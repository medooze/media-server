#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "mpeg4codec.h"
#include "video.h"

DWORD Mpeg4Decoder::bufSize = 4096*16;


//////////////////////////////////////////////////////////////////////////
//Mpeg4Decoder
// 	Decodificador MPEG4
//
//////////////////////////////////////////////////////////////////////////

/***********************
* Mpeg4Decoder
*	Consturctor
************************/
Mpeg4Decoder::Mpeg4Decoder()
{
	//Guardamos los valores por defecto
	codec = NULL;
	type = VideoCodec::MPEG4;
	bufLen = 0;
	
	//Encotramos el codec
	codec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);

	//Comprobamos
	if(codec==NULL)
	{
		Error("No decoder found\n");
		return ;
	}

	//Alocamos el contxto y el picture
	ctx = avcodec_alloc_context3(codec);
	picture = avcodec_alloc_frame();

	//POnemos los valores del contexto
	ctx->workaround_bugs 	= 255*255;
	ctx->error_concealment 	= FF_EC_GUESS_MVS | FF_EC_DEBLOCK;

	//Alocamos el buffer
	bufSize = 1024*756*3/2;
	buffer = (BYTE *)malloc(bufSize);
	frame = NULL;
	frameSize = 0;
	src = 0;

	//Lo abrimos
	avcodec_open2(ctx, codec, NULL);
}

/***********************
* ~Mpeg4Decoder
*	Destructor
************************/
Mpeg4Decoder::~Mpeg4Decoder()
{
	free(buffer);
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
int Mpeg4Decoder::DecodePacket(BYTE *in,DWORD len,int lost,int last)
{
	int ret = 1;

	//Check size
	if (bufLen+len>bufSize)
	{
		//Reset
		bufLen = 0;
		//Error
		return Error("-Not enought buffer size to decode packet");
	}
        //Guardamos
	memcpy(buffer+bufLen,in,len);
	
	//Aumentamos el tama�o
	bufLen+=len;
	
	//Y si es el ultimo
	if (last)
	{
		//Decode
		ret = Decode(buffer,bufLen);
		//Reset
		bufLen = 0;
	}

	return ret;
}

int Mpeg4Decoder::Decode(BYTE* buffer,DWORD size)
{
	int got_picture = 0;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = buffer;
	pkt.size = size;
	int readed = avcodec_decode_video2(ctx, picture, &got_picture, &pkt);

	//Si hay picture
	if (got_picture && readed>0)
	{
		if(ctx->width==0 || ctx->height==0)
			return Error("-Wrong dimmensions [%d,%d]\n",ctx->width,ctx->height);;

		int w = ctx->width;
		int h = ctx->height;
		int u = w*h;
		int v = w*h*5/4;
		int size = w*h*3/2;

		//Comprobamos el tama�o
		if (size>frameSize)
		{
			Log("-Frame size %dx%d\n",w,h);
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
	}
	return 1;
}


/******************************************************
* Mpeg4Encoder
*
*******************************************************/
DWORD Mpeg4Encoder::bufSize=4096*16;
/***********************
* Mpeg4Encoder
*	Constructor de la clase
************************/
Mpeg4Encoder::Mpeg4Encoder(const Properties& properties)
{
	// Set default values
	codec   = NULL;
	type    = VideoCodec::MPEG4;
	qMin	= 1;
	qMax	= 31;
	format  = 0;

	// Init framerate
	SetFrameRate(5,300,20);

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);

	// Check codec
	if(!codec)
		Error("No codec found");

	//No estamos abiertos
	opened = false;
 
	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);
	picture = avcodec_alloc_frame();

	//Y alocamos el buffer
	frame = new VideoFrame(type,bufSize);
}

/***********************
* ~Mpeg4Encoder
*	Destructor
************************/
Mpeg4Encoder::~Mpeg4Encoder()
{
	delete(frame);
	avcodec_close(ctx);
	free(ctx);
	free(picture);
}

/***********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
************************/
int Mpeg4Encoder::SetSize(int width, int height)
{
	Log("-SetSize [%d,%d]\n",width,height);

	// Set pixel format 
	ctx->pix_fmt		= PIX_FMT_YUV420P;
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
int Mpeg4Encoder::SetFrameRate(int frames,int kbits, int intraPeriod)
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
int Mpeg4Encoder::OpenCodec()
{
	// Check
	if (codec==NULL)
		return Error("No codec\n");

	// Check 
	if (opened)
		return Error("Already opened\n");

	// Bitrate,fps
	ctx->bit_rate 		= bitrate;
	ctx->bit_rate_tolerance = bitrate/fps+1;
	ctx->time_base          = (AVRational){1,fps};
	ctx->gop_size		= intraPeriod;	// about one Intra frame per second
	// Encoder quality
	ctx->rc_max_rate	= bitrate;
	ctx->rc_buffer_size	= bitrate/fps+1;
	ctx->rc_initial_buffer_occupancy = 0;
	ctx->rc_qsquish 	= 1;
	ctx->max_b_frames	= 0;

	// Open codec
	if (avcodec_open2(ctx, codec, NULL)<0)
		return Error("Unable to open H263 codec\n");

	// We are opened
	opened=true;

	// Exit
	return 1;
}


/***********************
* EncodeFrame
*	Codifica un frame
************************/
VideoFrame* Mpeg4Encoder::EncodeFrame(BYTE *in,DWORD len)
{
	int numPixels = ctx->width*ctx->height;

	//Comprobamos el tama�o
	if (numPixels*3/2 != len)
		return NULL;

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
	//Clean all previous packets
	frame->ClearRTPPacketizationInfo();

	//From the begining
	DWORD ini = 0;

	//Copy all
	while(ini<bufLen)
	{
		//The mtu
		DWORD len = RTPPAYLOADSIZE;

		//Check length
		if (len+ini>bufLen)
			//Fix it
			len=bufLen-ini;

		//Add rtp packet
		frame->AddRtpPacket(ini,len,NULL,0);

		//Increase pointer
		ini += len;
	}

	//Y ponemos a cero el comienzo
	bufIni=0;

	return frame;
}

/***********************
* FastPictureUpdate
*	Manda un frame entero
************************/
int Mpeg4Encoder::FastPictureUpdate()
{
	//If we have picture
	if (picture)
	{
		//Set it
		picture->key_frame = 1;
		picture->pict_type = AV_PICTURE_TYPE_I;
	}
	return 1;
}
