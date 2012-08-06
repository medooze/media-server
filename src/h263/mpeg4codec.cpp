#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "mpeg4codec.h"
#include "video.h"

#define CODEC_FLAG_RFC2190         0x04000000
#define VIDEOQMIN 3
#define VIDEOQMAX 31


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
	codec = avcodec_find_decoder(CODEC_ID_MPEG4);

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


	//Lo abrimos
	avcodec_open2(ctx, codec, NULL);
}

/***********************
* ~Mpeg4Decoder
*	Destructor
************************/
Mpeg4Decoder::~Mpeg4Decoder()
{
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
	int ret = 0;

        //Guardamos
	memcpy(bufDecode+bufLen,in,len);
	
	//Aumentamos el tama�o
	bufLen+=len;
	
	//Y si es el ultimo
	if (last)
	{
		//Decode
		ret = Decode(bufDecode,bufLen);
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
	avcodec_decode_video2(ctx, picture, &got_picture, &pkt);

	//Si hay picture
	if (got_picture)
	{
		if(ctx->width==0 || ctx->height==0)
			return 0;

		int w = ctx->width;
		int h = ctx->height;
		int u = w*h;
		int v = w*h*5/4;

		//Copaamos  el Cy
		for(int i=0;i<ctx->height;i++)
			memcpy(&bufDecode[i*w],&picture->data[0][i*picture->linesize[0]],w);

		//Y el Cr y Cb
		for(int i=0;i<ctx->height/2;i++)
		{
			memcpy(&bufDecode[i*w/2+u],&picture->data[1][i*picture->linesize[1]],w/2);
			memcpy(&bufDecode[i*w/2+v],&picture->data[2][i*picture->linesize[2]],w/2);
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
Mpeg4Encoder::Mpeg4Encoder(int qualityMin,int qualityMax)
{
	// Set default values
	codec   = NULL;
	type    = VideoCodec::MPEG4;
	qMin	= qualityMin;
	qMax	= qualityMax;
	format  = 0;

	// Init framerate
	SetFrameRate(5,300,20);

	// Get encoder
	codec = avcodec_find_encoder(CODEC_ID_MPEG4);

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
	ctx->bit_rate_tolerance = 1;
	ctx->time_base          = (AVRational){1,fps};
	ctx->gop_size		= intraPeriod;	// about one Intra frame per second
	// Encoder quality
	ctx->rc_min_rate 	= bitrate;
	ctx->rc_max_rate	= bitrate;
	ctx->rc_buffer_size	= bufSize;
	ctx->rc_qsquish 	= 0; //ratecontrol qmin qmax limiting method.
	ctx->max_b_frames	= 0;
	ctx->qmin= qMin;
	ctx->qmax= qMax;
	ctx->i_quant_factor	= (float)-0.6;
	ctx->i_quant_offset 	= (float)0.0;
	ctx->b_quant_factor 	= (float)1.5;
	// Flags
	ctx->flags |= CODEC_FLAG_PASS1;			//PASS1 

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

/***********************
* GetNextPacket
*	Obtiene el siguiente paquete para enviar
************************/
int Mpeg4Encoder::GetNextPacket(BYTE *out,DWORD &len)
{
	//Copiamos lo que quepa
	if (len+bufIni>bufLen)
		len=bufLen-bufIni;

	//Copiamos
	memcpy(out,frame->GetData()+bufIni,len);

	//Incrementamos el inicio
	bufIni+=len;

	//Y salimos
	return bufLen>bufIni;
}

