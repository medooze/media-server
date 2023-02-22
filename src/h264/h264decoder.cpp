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
	if(!codec)
	{
		Error("H264Decoder::H264Decoder() | No decoder found\n");
		return;
	}

	//Get codec context
	ctx = avcodec_alloc_context3(codec);
	//Create packet
	packet = av_packet_alloc();
	//Allocate frame
	picture = av_frame_alloc();

	//Open codec
	avcodec_open2(ctx, codec, NULL);
}

H264Decoder::~H264Decoder()
{
	if (ctx)
	{
		avcodec_close(ctx);
		free(ctx);
	}
	if (packet)
		//Release pacekt
		av_packet_free(&packet);
	if (picture)
		//Release frame
		av_frame_free(&picture);
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
	//Set data
	packet->data = (uint8_t*)data;
	packet->size = size;

	//Decode it
	if (avcodec_send_packet(ctx, packet) < 0)
		//Error
		return Error("-H264Decoder::Decode() Error decoding H264 packet\n");

	//Check if we got any decoded frame
	if (avcodec_receive_frame(ctx, picture) <0)
		//No frame decoded yet
		return 0;
	
	if(ctx->width==0 || ctx->height==0)
		return Error("-H264Decoder::Decode() | Wrong dimmensions [%d,%d]\n",ctx->width,ctx->height);

	//Set new size in pool
	videoBufferPool.SetSize(ctx->width, ctx->height);

	//Get new frame
	videoBuffer = videoBufferPool.allocate();

	//Set interlaced flags
	videoBuffer->SetInterlaced(picture->interlaced_frame);

	//Get planes
	Plane& y = videoBuffer->GetPlaneY();
	Plane& u = videoBuffer->GetPlaneU();
	Plane& v = videoBuffer->GetPlaneV();
		
	//Copaamos  el Cy
	for (uint32_t i = 0; i < std::min<uint32_t>(ctx->height, y.GetHeight()); i++)
		memcpy(y.GetData() + i * y.GetStride(), &picture->data[0][i * picture->linesize[0]], y.GetWidth());

	//Y el Cr y Cb
	for (uint32_t i = 0; i < std::min<uint32_t>({ ctx->height / 2, u.GetHeight(), v.GetHeight() }); i++)
	{
		memcpy(u.GetData() + i * u.GetStride(), &picture->data[1][i * picture->linesize[1]], u.GetWidth());
		memcpy(v.GetData() + i * v.GetStride(), &picture->data[2][i * picture->linesize[2]], v.GetWidth());
	}

	//OK
	return 1;
}

