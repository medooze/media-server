#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "h263codec.h"
#include "video.h"
#include "av_codec_encode_video.h"


//////////////////////////////////////////////////////////////////////////
//Encoder
// 	Codificador H263
//
//////////////////////////////////////////////////////////////////////////
/***********************
* H263Encoder
*	Constructor de la clase
************************/
H263Encoder::H263Encoder(const Properties& properties)
{
	// Set default values
	frame	= NULL;
	codec   = NULL;
	type    = VideoCodec::H263_1998;
	format  = 0;

	// Init framerate
	SetFrameRate(5,300,20);

	// Get encoder
	codec = avcodec_find_encoder(AV_CODEC_ID_H263P);

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
* ~H263Encoder
*	Destructor
************************/
H263Encoder::~H263Encoder()
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
int H263Encoder::SetSize(int width, int height)
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
int H263Encoder::SetFrameRate(int frames,int kbits,int intraPeriod)
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
int H263Encoder::OpenCodec()
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
VideoFrame* H263Encoder::EncodeFrame(BYTE *in,DWORD len)
{
	//Check if we are opened
	if (!opened)
		return NULL;
	
	int numPixels = ctx->width*ctx->height;

	//Comprobamos el tama�o
	if (numPixels*3/2 != len)
		return NULL;

	//POnemos los valores
	picture->data[0] = in;
	picture->data[1] = in+numPixels;
	picture->data[2] = in+numPixels*5/4;

	//Codificamos
	int ret = avcodec_encode_video(ctx,frame->GetData(),frame->GetMaxMediaLength(),picture);

	//Check
	if (ret<0)
	{
		//Error
		Error("%d\n",frame->GetMaxMediaLength());
		//Exit
		return NULL;
	}

	//Set lenfht
	DWORD bufLen = ret;

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

	//Skip first two
	DWORD ini = 2;

	//The prefix
	BYTE prefix[2];
	//Set header for first
	prefix[0] = 0x04;
	prefix[1] = 0x00;
	
	//Clean all previous packets
	frame->ClearRTPPacketizationInfo();

	//Copy all
	while(ini<bufLen)
	{
		//The mtu
		DWORD len = RTPPAYLOADSIZE-2;

		//Check length
		if (len+ini>bufLen)
			//Fix it
			len=bufLen-ini;
		
		//Add rtp packet
		frame->AddRtpPacket(ini,len,prefix,2);

		//If it is first
		if (ini==2)
		{
			//Set header for the nexts
			prefix[0] = 0x00;
			prefix[1] = 0x00;
		}
		
		//Increase pointer
		ini += len;
	}
	
	return frame;
}

/***********************
* FastPictureUpdate
*	Manda un frame entero
************************/
int H263Encoder::FastPictureUpdate()
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

//////////////////////////////////////////////////////////////////////////
//H263Decoder
// 	Decodificador H263
//
//////////////////////////////////////////////////////////////////////////
/***********************
* H263Decoder
*	Consturctor
************************/
H263Decoder::H263Decoder()
{
	//Guardamos los valores por defecto
	codec = NULL;
	type = VideoCodec::H263_1998;
	bufLen = 0;

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
	picture = av_frame_alloc();

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
* ~H263Decoder
*	Destructor
************************/
H263Decoder::~H263Decoder()
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
int H263Decoder::DecodePacket(BYTE *in,DWORD inLen,int lost,int last)
{
	int ret = 1;

	// Check total length
	if (bufLen+inLen+FF_INPUT_BUFFER_PADDING_SIZE+2>bufSize)
	{
		Log("-H263 decode buffer size error, reseting\n");

		// Reset buffer
		bufLen = 0;

		// Exit
		return 0;
	}

	//Check if not empty last pacekt
	if (inLen)
	{

		/* The H.263+ payload header is structured as follows:

		      0                   1
		      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
		     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		     |   RR    |P|V|   PLEN    |PEBIT|
		     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

		   RR: 5 bits
		     Reserved bits.  Shall be zero.

		   P: 1 bit
		     Indicates the picture start or a picture segment (GOB/Slice) start
		     or a video sequence end (EOS or EOSBS).  Two bytes of zero bits
		     then have to be prefixed to the payload of such a packet to compose
		     a complete picture/GOB/slice/EOS/EOSBS start code.  This bit allows
		     the omission of the two first bytes of the start codes, thus
		     improving the compression ratio.

		   V: 1 bit
		     Indicates the presence of an 8 bit field containing information for
		     Video Redundancy Coding (VRC), which follows immediately after the
		     initial 16 bits of the payload header if present.  For syntax and
		     semantics of that 8 bit VRC field see section 4.2.

		   PLEN: 6 bits
		     Length in bytes of the extra picture header.  If no extra picture
		     header is attached, PLEN is 0.  If PLEN>0, the extra picture header
		     is attached immediately following the rest of the payload header.
		     Note the length reflects the omission of the first two bytes of the
		     picture start code (PSC).  See section 5.1.

		   PEBIT: 3 bits
		     Indicates the number of bits that shall be ignored in the last byte
		     of the picture header.  If PLEN is not zero, the ignored bits shall
		     be the least significant bits of the byte.  If PLEN is zero, then
		     PEBIT shall also be zero.
		*/

		/* Get header */
		BYTE p = in[0] & 0x04;
		BYTE v = in[0] & 0x02;
		BYTE plen = ((in[0] & 0x1 ) << 5 ) | (in[1] >> 3);
		BYTE pebit = in[0] & 0x7;

		/* Skip the header and the extra picture */
		BYTE* i = in+2+plen;
		DWORD  len = inLen-2-plen;

		/* Check extra VRC byte*/
		if (v)
		{
			/* Increase ini */
			i++;
			len--;
		}

		/* Check p bit for first packet of a frame*/
		if (p)
		{
			/* Append 0s to the begining*/
			buffer[bufLen] = 0;
			buffer[bufLen+1] = 0;
			/* Increase buffer length*/
			bufLen += 2;
		}

		/* Copy the rest */
		memcpy(buffer+bufLen,i,len);

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

int H263Decoder::Decode(BYTE *buffer,DWORD size)
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
