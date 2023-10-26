#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <memory.h>
#include "log.h"
#include "JPEGEncoder.h"

/**********************
* JPEGEncoder
*	Constructor de la clase
***********************/
JPEGEncoder::JPEGEncoder(const Properties& properties) : frame(VideoCodec::JPEG)
{
	// Set default values

	type = VideoCodec::JPEG;
	//No bitrate
	bitrate = 0;
	fps = 0;

	//Disable sharing buffer on clone
	frame.DisableSharedBuffer();
}

JPEGEncoder::~JPEGEncoder()
{
	if (codec) avcodec_close(ctx);
	if (ctx) av_free(ctx);
	if (input) av_frame_free(&input);
}


int JPEGEncoder::SetSize(int width, int height)
{
	//Check if they are the same size
	if (this->width == width && this->height == height)
		//Do nothing
		return 1;
	
	Log("-JPEGEncoder::SetSize() [width:%d,height:%d]\n", width, height);

	//Save values
	this->width = width;
	this->height = height;

	// Open codec
	return OpenCodec();
}

/************************
* SetFrameRate
* 	Indica el numero de frames por segudno actual
**************************/
int JPEGEncoder::SetFrameRate(int frames, int kbits, int intraPeriod)
{
	// Save frame rate
	if (frames > 0)
		fps = frames;
	else
		fps = 10;

	// Save bitrate
	if (kbits > 0)
		bitrate = kbits;

	return 1;
}

/**********************
* OpenCodec
*	Abre el codec
***********************/
int JPEGEncoder::OpenCodec()
{
	Log("-JPEGEncoder::OpenCodec() [%dkbps,%dfps,width:%d,height:%d]\n", bitrate, fps, width, height);

	// Check 
	if (codec)
		return Error("Codec already opened\n");

	//Open MJPEG codec
	if (!(codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG)))
		return Error("Codec not found\n");

	//Allocate codec contex
	if (!(ctx = avcodec_alloc_context3(codec)))
		return Error("Could not allocate video codec context\n");

	//Set properties
	ctx->bit_rate = bitrate * 1000;
	ctx->width = width;
	ctx->height = height;
	ctx->time_base = { 1, fps };
	ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;

	//Open encoder
	if (avcodec_open2(ctx, codec, NULL) < 0)
		return Error("Could not open codec\n");
	
	//Allocate frame
	if (!(input = av_frame_alloc()))
		return Error("Could not allocate video frame\n");

	// Exit
	return 1;
}


/**********************
* EncodeFrame
*	Codifica un frame
***********************/
VideoFrame* JPEGEncoder::EncodeFrame(const VideoBuffer::const_shared& videoBuffer)
{
	if (!codec)
	{
		Error("-JPEGEncoder::EncodeFrame() | Codec not opened\n");
		return nullptr;
	}

	//Get planes
	const Plane& y = videoBuffer->GetPlaneY();
	const Plane& u = videoBuffer->GetPlaneU();
	const Plane& v = videoBuffer->GetPlaneV();

	//Set input data
	input->data[0] = (unsigned char*)y.GetData();
	input->data[1] = (unsigned char*)u.GetData();
	input->data[2] = (unsigned char*)v.GetData();
	input->data[3] = nullptr;
	input->linesize[0] = y.GetStride();
	input->linesize[1] = u.GetStride();
	input->linesize[2] = v.GetStride();
	input->linesize[3] = 0;
	input->format = AV_PIX_FMT_YUVJ420P;
	input->width  = width;
	input->height = height;
	input->pts = 1;

	//Set color range
	switch (videoBuffer->GetColorRange())
	{
		case VideoBuffer::ColorRange::Partial:
			//219*2^(n-8) "MPEG" YUV ranges
			input->color_range = AVCOL_RANGE_MPEG;
			break;
		case VideoBuffer::ColorRange::Full:
			//2^n-1   "JPEG" YUV ranges
			input->color_range = AVCOL_RANGE_JPEG;
			break;
		default:
			//Unknown
			input->color_range = AVCOL_RANGE_UNSPECIFIED;
	}

	//Get color space
	switch (videoBuffer->GetColorSpace())
	{
		case VideoBuffer::ColorSpace::SRGB:
			///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
			input->colorspace = AVCOL_SPC_RGB;
			break;
		case VideoBuffer::ColorSpace::BT709:
			///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
			input->colorspace = AVCOL_SPC_BT709;
			break;
		case VideoBuffer::ColorSpace::BT601:
			///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
			input->colorspace = AVCOL_SPC_BT470BG;
			break;
		case VideoBuffer::ColorSpace::SMPTE170:
			///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
			input->colorspace = AVCOL_SPC_SMPTE170M;
			break;
		case VideoBuffer::ColorSpace::SMPTE240:
			///< functionally identical to above
			input->colorspace = AVCOL_SPC_SMPTE240M;
			break;
		case VideoBuffer::ColorSpace::BT2020:
			///< ITU-R BT2020 non-constant luminance system
			input->colorspace = AVCOL_SPC_BT2020_NCL;
			break;
		default:
			//Unknown
			input->colorspace = AVCOL_SPC_UNSPECIFIED;
	}

	//Init ouput data
	std::shared_ptr<AVPacket> packet(
		av_packet_alloc(),
		[](auto packet) {av_packet_free(&packet); }
	);
	
	//Send frame for encoding
	if (avcodec_send_frame(ctx, input) !=0 )
	{
		Error("-JPEGEncoder::EncodeFrame() | Error encoding frame\n");
		return nullptr;
	}
	
	//Receive encoded packet
	if (avcodec_receive_packet(ctx, packet.get()) != 0)
		return nullptr;

	//Set the media
	frame.SetMedia(packet->data, packet->size);

	//Set width and height
	frame.SetWidth(width);
	frame.SetHeight(height);

	//Set intra
	frame.SetIntra(true);

	//Emtpy rtp info
	frame.ClearRTPPacketizationInfo();

	return &frame;
}

int JPEGEncoder::FastPictureUpdate()
{
	return 1;
}

