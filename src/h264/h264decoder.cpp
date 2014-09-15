#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "h264decoder.h"

//H264Decoder
// 	Decodificador H264
//
//////////////////////////////////////////////////////////////////////////
/***********************
* H264Decoder
*	Consturctor
************************/
H264Decoder::H264Decoder()
{
	type = VideoCodec::H264;

	//Guardamos los valores por defecto
	codec = NULL;
	buffer = 0;
	bufLen = 0;
	bufSize = 0;
	ctx = NULL;
	picture = NULL;

	//Encotramos el codec
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);

	//Comprobamos
	if(codec==NULL)
	{
		Error("No decoder found\n");
		return;
	}

	//Alocamos el contxto y el picture
	ctx = avcodec_alloc_context3(codec);
	ctx->flags != CODEC_FLAG_EMU_EDGE;
	picture = av_frame_alloc();

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
* ~H264Decoder
*	Destructor
************************/
H264Decoder::~H264Decoder()
{
	if (buffer)
		free(buffer);
	if (frame)
		free(frame);
	if (ctx)
	{
		avcodec_close(ctx);
		free(ctx);
	}
	if (picture)
		free(picture);
	
}
/* 3 zero bytes syncword */
static const uint8_t sync_bytes[] = { 0, 0, 0, 1 };



DWORD h264_append_nals(BYTE *dest, DWORD destLen, DWORD destSize, BYTE *buffer, DWORD bufferLen, BYTE **nals, DWORD nalSize, DWORD *num)
{
	BYTE nal_unit_type;
	unsigned int header_len;
	BYTE nal_ref_idc;
	unsigned int nalu_size;

	DWORD payload_len = bufferLen;
	BYTE *payload = buffer;
	BYTE *outdata = dest+destLen;
	DWORD outsize = 0;

	//No nals
	*num = 0;

	//Check
	if (!bufferLen)
		//Exit
		return 0;


	/* +---------------+
	 * |0|1|2|3|4|5|6|7|
	 * +-+-+-+-+-+-+-+-+
	 * |F|NRI|  Type   |
	 * +---------------+
	 *
	 * F must be 0.
	 */
	nal_ref_idc = (payload[0] & 0x60) >> 5;
	nal_unit_type = payload[0] & 0x1f;
	//printf("[NAL:%x,type:%x]\n", payload[0], nal_unit_type);

	/* at least one byte header with type */
	header_len = 1;

	switch (nal_unit_type) 
	{
		case 0:
		case 30:
		case 31:
			/* undefined */
			return 0;
		case 25:
			/* STAP-B		Single-time aggregation packet		 5.7.1 */
			/* 2 byte extra header for DON */
			/** Not supported */
			return 0;	
		case 24:
		{
			/**
			   Figure 7 presents an example of an RTP packet that contains an STAP-
			   A.  The STAP contains two single-time aggregation units, labeled as 1
			   and 2 in the figure.

			       0                   1                   2                   3
			       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			      |                          RTP Header                           |
			      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			      |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
			      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			      |                         NALU 1 Data                           |
			      :                                                               :
			      +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			      |               | NALU 2 Size                   | NALU 2 HDR    |
			      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			      |                         NALU 2 Data                           |
			      :                                                               :
			      |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			      |                               :...OPTIONAL RTP padding        |
			      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

			      Figure 7.  An example of an RTP packet including an STAP-A and two
					 single-time aggregation units
			*/

			/* Skip STAP-A NAL HDR */
			payload++;
			payload_len--;
			
			/* STAP-A Single-time aggregation packet 5.7.1 */
			while (payload_len > 2) 
			{
				/* Get NALU size */
				nalu_size = (payload[0] << 8) | payload[1];

				/* strip NALU size */
				payload += 2;
				payload_len -= 2;

				if (nalu_size > payload_len)
					nalu_size = payload_len;

				outsize += nalu_size + sizeof (sync_bytes);

				/* Check size */
				if (outsize + destLen >destSize)
					return Error("Frame to small to add NAL [%d,%d,%d]\n",outsize,destLen,destSize);

				memcpy (outdata, sync_bytes, sizeof (sync_bytes));
				outdata += sizeof (sync_bytes);

				//Set nal
				if (nals && nalSize-1>*num)
					//Add it
					nals[(*num)++] = outdata;

				memcpy (outdata, payload, nalu_size);
				outdata += nalu_size;

				payload += nalu_size;
				payload_len -= nalu_size;
			}

			return outsize;
		}
		case 26:
			/* MTAP16 Multi-time aggregation packet	5.7.2 */
			header_len = 5;
			return 0;
			break;
		case 27:
			/* MTAP24 Multi-time aggregation packet	5.7.2 */
			header_len = 6;
			return 0;
			break;
		case 28:
		case 29:
		{
			/* FU-A	Fragmentation unit	 5.8 */
			/* FU-B	Fragmentation unit	 5.8 */
			BYTE S, E;

			/* +---------------+
			 * |0|1|2|3|4|5|6|7|
			 * +-+-+-+-+-+-+-+-+
			 * |S|E|R| Type	   |
			 * +---------------+
			 *
			 * R is reserved and always 0
			 */
			S = (payload[1] & 0x80) == 0x80;
			E = (payload[1] & 0x40) == 0x40;

			if (S) 
			{
				/* NAL unit starts here */
				BYTE nal_header;

				/* reconstruct NAL header */
				nal_header = (payload[0] & 0xe0) | (payload[1] & 0x1f);

				/* strip type header, keep FU header, we'll reuse it to reconstruct
				 * the NAL header. */
				payload += 1;
				payload_len -= 1;

				nalu_size = payload_len;
				outsize = nalu_size + sizeof (sync_bytes);

				//Check size
				if (outsize + destLen >destSize)
					return Error("Frame too small to add NAL [%d,%d,%d]\n",outsize,destLen,destSize);
				
				memcpy (outdata, sync_bytes, sizeof (sync_bytes));
				outdata += sizeof (sync_bytes);
				
				//Set nal
				if (nals && nalSize-1>*num)
					//Add it
					nals[(*num)++] = outdata;

				memcpy (outdata, payload, nalu_size);
				outdata[0] = nal_header;
				outdata += nalu_size;
				return outsize;

			} else {
				/* strip off FU indicator and FU header bytes */
				payload += 2;
				payload_len -= 2;

				outsize = payload_len;
				//Check size
				if (outsize + destLen >destSize)
					return Error("Frame too small to add NAL [%d,%d,%d]\n",outsize,destLen,destSize);
				memcpy (outdata, payload, outsize);
				outdata += nalu_size;
				return outsize;
			}

			break;
		}
		default:
		{
			/* 1-23	 NAL unit	Single NAL unit packet per H.264	 5.6 */
			/* the entire payload is the output buffer */
			nalu_size = payload_len;
			outsize = nalu_size + sizeof (sync_bytes);
			//Check size
			if (outsize + destLen >destSize)
				return Error("Frame too small to add NAL [%d,%d,%d]\n",outsize,destLen,destSize);
			memcpy (outdata, sync_bytes, sizeof (sync_bytes));
			outdata += sizeof (sync_bytes);

			//Set nal
			if (nals && nalSize-1>*num)
				//Add it
				nals[(*num)++] = outdata;

			memcpy (outdata, payload, nalu_size);
			outdata += nalu_size;

			return outsize;
		}
	}

	return 0;
}

DWORD h264_append(BYTE *dest, DWORD destLen, DWORD destSize, BYTE *buffer, DWORD bufferLen)
{
	DWORD num = 0;
	return h264_append_nals(dest,destLen,destSize,buffer,bufferLen,NULL,0,&num);
}

/***********************
* DecodePacket 
*	Decodifica un packete
************************/
int H264Decoder::DecodePacket(BYTE *in,DWORD inLen,int lost,int last)
{
	int ret = 1;
	
	// Check total length
	if (bufLen+inLen+FF_INPUT_BUFFER_PADDING_SIZE>bufSize)
	{
		// Reset buffer
		bufLen = 0;

		// Exit
		return 0;
	}

	//Aumentamos la longitud
	bufLen += h264_append(buffer,bufLen,bufSize-FF_INPUT_BUFFER_PADDING_SIZE,in,inLen);

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

int H264Decoder::Decode(BYTE *buffer,DWORD size)
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
			return Error("-Wrong dimmensions [%d,%d]\n",ctx->width,ctx->height);

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

