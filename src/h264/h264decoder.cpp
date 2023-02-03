#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "log.h"
#include "h264decoder.h"

H264Decoder::H264Decoder() :
	VideoDecoder(VideoCodec::H264),
	depacketizer(true),
	videoBufferPool(2,4)
{
	//Open libavcodec
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);

	//Check
	if(codec==NULL)
	{
		Error("No decoder found\n");
		return;
	}

	//Alloc and open context and pictture
	ctx = avcodec_alloc_context3(codec);
	picture = av_frame_alloc();
	avcodec_open2(ctx, codec, NULL);
}

H264Decoder::~H264Decoder()
{
	if (ctx)
	{
		avcodec_close(ctx);
		free(ctx);
	}
	if (picture)
		free(picture);
}

int H264Decoder::DecodePacket(const BYTE* data, DWORD size, int lost, int last)
{

	int ret = 1;

	//Add to 
	VideoFrame* frame = (VideoFrame*)depacketizer.AddPayload(data, size);

	//Check last mark
	if (last)
	{
		//If got frame
		if (frame)
			//Decode it
			ret = Decode(frame->GetData(), frame->GetLength());
		//Reset frame
		depacketizer.ResetFrame();
	}

	//Return ok
	return ret;
}

int H264Decoder::Decode(const BYTE *data,DWORD size)
{
	//Decodificamos
	int got_picture=0;
	//Decodificamos
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = (uint8_t*)data;
	pkt.size = size;
	int readed = avcodec_decode_video2(ctx, picture, &got_picture, &pkt);

	//Si hay picture
	if (got_picture && readed>0)
	{
		if(ctx->width==0 || ctx->height==0)
			return Error("-Wrong dimmensions [%d,%d]\n",ctx->width,ctx->height);

		//Set new size in pool
		videoBufferPool.SetSize(ctx->width, ctx->height);

		//Get new frame
		videoBuffer = videoBufferPool.allocate();

		//Get planes
		Plane& y = videoBuffer->GetPlaneY();
		Plane& u = videoBuffer->GetPlaneU();
		Plane& v = videoBuffer->GetPlaneV();
		
		//Copaamos  el Cy
		for(int i=0;i<std::min<uint32_t>(ctx->height, y.GetHeight()) ; i++)
			memcpy(y.GetData() + i * y.GetStride(), &picture->data[0][i * picture->linesize[0]], y.GetWidth());

		//Y el Cr y Cb
		for(int i=0;i< std::min<uint32_t>({ctx->height/2, u.GetHeight(), v.GetHeight()});i++)
		{
			memcpy(u.GetData() + i * u.GetStride(), &picture->data[1][i * picture->linesize[1]], u.GetWidth());
			memcpy(v.GetData() + i * v.GetStride(), &picture->data[2][i * picture->linesize[2]], v.GetWidth());
		}
	}
	return 1;
}

