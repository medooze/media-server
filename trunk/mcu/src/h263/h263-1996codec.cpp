#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "h263codec.h"

DWORD H263Decoder1996::bufSize = 4096*16;

/***********************
* H263Decoder1996
*	Consturctor
************************/
H263Decoder1996::H263Decoder1996()
{
	//Guardamos los valores por defecto
	codec = NULL;
	type = VideoCodec::H263_1996;
	bufLen = 0;
	
	//Registramos todo
	avcodec_register_all();

	//Encotramos el codec
	codec = avcodec_find_decoder(AV_CODEC_ID_H263);

	//Comprobamos
	if(codec==NULL)
	{
		Error("No decoder found\n");
		return ;
	}

	//Alocamos el contxto y el picture
	ctx = avcodec_alloc_context3(codec);
	picture = avcodec_alloc_frame();

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
* ~H263Decoder1996
*	Destructor
************************/
H263Decoder1996::~H263Decoder1996()
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
int H263Decoder1996::DecodePacket(BYTE *in,DWORD len,int lost,int last)
{
	int ret = 1;

	//Check not empty last packet
	if (len)
	{
		//Comprobamos que quepa
		if(len<4)
			return Error("Recived short packed [%d]\n",len);

		//Get header
		H263HeadersBasic* headers = H263HeadersBasic::CreateHeaders(in[0]);

		//Parse it
		DWORD parsed = headers->Parse(in,len);

		//Check
		if (!parsed)
			return Error("Error parsing H263 RFC 2190 packet\n");

		//Si ha cambiado el formato
		if (!src)
		{
			//Guardamos el formato
			src = headers->src;
		} else if(src!=headers->src) {
			Log("-Source format changed, reopening codec [%d,%d]\n",src,headers->src);

			//Cerramos el codec
			avcodec_close(ctx);

			//Y lo reabrimos
			avcodec_open2(ctx, codec, NULL);

			//Guardamos el formato
			src = headers->src;

			//Nos cargamos el buffer
			bufLen = 0;
		}

		//Aumentamos el puntero
		in+=parsed;
		len-=parsed;

		//POnemos en blanco el primer bit hasta el comienzo
		in[0] &= 0xff >> headers->sbits;

		//Y el final
		if (len>0)
			in[len-1] &= 0xff << headers->ebits;

		//Si el hay solapamiento de bytes
		if(headers->sbits!=0 && bufLen>0)
		{
			//A�adimos lo que falta
			buffer[bufLen-1] |= in[0];

			//Y avanzamos
			in++;
			len--;
		}

		//Free headers
		delete(headers);

		//Nos aseguramos que quepa
		if (len<0 || bufLen+len+FF_INPUT_BUFFER_PADDING_SIZE>bufSize)
			return Error("Wrong size of packet [%d,%d]\n",bufLen,len);

		//Copiamos
		memcpy(buffer+bufLen,in,len);

		//Aumentamos la longitud
		bufLen+=len;
	}

	//Si es el ultimo
	if(last)
	{
		//Borramos el final
		memset(buffer+bufLen,0,FF_INPUT_BUFFER_PADDING_SIZE);

		//Decode
		ret = Decode(buffer,bufLen);
		//Y resetamos el buffer
		bufLen=0;
	}
	//Return
	return ret;
}

int H263Decoder1996::Decode(BYTE *buffer,DWORD size)
{
	//Decodificamos
	int got_picture=0;
	//Decodificamos
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = buffer;
	pkt.size = size;
	int readed = avcodec_decode_video2(ctx, picture, &got_picture, &pkt);


	//Si hay picture
	if (got_picture && readed>0)
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

//////////////////////////////////////////////////////////////////////////
//Encoder
// 	Codificador H263
//
//////////////////////////////////////////////////////////////////////////
/***********************
* H263Encoder
*	Constructor de la clase
************************/
H263Encoder1996::H263Encoder1996(const Properties& properties)
{
	// Set default values
	frame	= NULL;
	codec   = NULL;
	type    = VideoCodec::H263_1998;
	format  = 0;

	// Init framerate
	SetFrameRate(5,300,20);

	//Register all
	avcodec_register_all();

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_H263);

	// Check codec
	if(!codec)
		Error("No codec found\n");

	//No estamos abiertos
	opened = false;

	//Alocamos el conto y el picture
	ctx = avcodec_alloc_context3(codec);
	picture = avcodec_alloc_frame();
}

/***********************
* ~H263Encoder
*	Destructor
************************/
H263Encoder1996::~H263Encoder1996()
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
int H263Encoder1996::SetSize(int width, int height)
{
	Log("-SetSize [%d,%d]\n",width,height);

	//Check size
	if (!(
		(width==128 && height==96)  ||
		(width==176 && height==144) ||
		(width==352 && height==288) ||
		(width==704 && height==576) ||
		(width==1408 && height==5711526)
		))
		//Error
		return Error("Size not supported for H263");

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
int H263Encoder1996::SetFrameRate(int frames,int kbits,int intraPeriod)
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
int H263Encoder1996::OpenCodec()
{
	Log("-OpenCodec H263 [%dbps,%dfps]\n",bitrate,fps);

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
	DWORD bufSize = 1.5*bitrate/fps;

	//Check size
	if (bufSize<FF_MIN_BUFFER_SIZE)
		//Set minimun
		bufSize = FF_MIN_BUFFER_SIZE;

	//Y alocamos el buffer
	frame = new VideoFrame(type,bufSize);

	// Bitrate,fps
	ctx->bit_rate 		= bitrate;
	ctx->bit_rate_tolerance = bitrate/fps+1;
	ctx->time_base          = (AVRational){1,fps};
	ctx->gop_size		= intraPeriod;

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
VideoFrame* H263Encoder1996::EncodeFrame(BYTE *in,DWORD len)
{
	//Check we are opened
	if (!opened)
		//Error
		return NULL;

	//Get number of pixels in image
	int numPixels = ctx->width*ctx->height;

	//Comprobamos el tama�o
	if (numPixels*3/2 != len)
		//Error
		return NULL;

	//POnemos los valores
	picture->data[0] = in;
	picture->data[1] = in+numPixels;
	picture->data[2] = in+numPixels*5/4;

	//Clean all previous packets
	frame->ClearRTPPacketizationInfo();

	//Codificamos
	DWORD bufLen = avcodec_encode_video(ctx,frame->GetData(),frame->GetMaxMediaLength(),picture);

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

	//Paquetize
	paquetizer.PaquetizeFrame(frame);
	
	//From the first
	num = 0;
	
	return frame;
}

/***********************
* FastPictureUpdate
*	Manda un frame entero
************************/
int H263Encoder1996::FastPictureUpdate()
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

