/* 
 * File:   vp8encoder.cpp
 * Author: Sergio
 * 
 * Created on 13 de noviembre de 2012, 15:57
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "vp8encoder.h"
#include "vp8.h"


#define interface (vpx_codec_vp8_cx())

#ifndef VPX_PLANE_Y
#define VPX_PLANE_Y   0
#endif
#ifndef VPX_PLANE_U
#define VPX_PLANE_U   1
#endif
#ifndef VPX_PLANE_V
#define VPX_PLANE_V   2
#endif


VP8Encoder::VP8Encoder(const Properties& properties)
{
	// Set default values
	type    = VideoCodec::VP8;
	frame	= NULL;
	pts	= 0;
	num	= 0;
	pic	= NULL;
	//not force
	forceKeyFrame = false;
	
	//No estamos abiertos
	opened = false;

	//No bitrate
	bitrate = 0;
	fps = 0;
	intraPeriod = 0;
}

VP8Encoder::~VP8Encoder()
{
	vpx_codec_destroy(&encoder);
	if (pic)
		vpx_img_free(pic);
	if (frame)
		delete frame;
}

/**********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
***********************/
int VP8Encoder::SetSize(int width, int height)
{
	//Save values
	this->width = width;
	this->height = height;

	//calculate number of pixels in  input image
	numPixels = width*height;

	// Open codec
	return OpenCodec();
}

/************************
* SetFrameRate
* 	Indica el numero de frames por segudno actual
**************************/
int VP8Encoder::SetFrameRate(int frames,int kbits,int intraPeriod)
{
	// Save frame rate
	if (frames>0)
		fps=frames;
	else
		fps=10;

	// Save bitrate
	if (kbits>0)
		bitrate=kbits;

	//Save intra period
	if (intraPeriod>0)
		this->intraPeriod = intraPeriod;

	//Check if already opened
	if (opened)
	{
		//Reconfig parameters
		config.rc_target_bitrate = bitrate;
		config.kf_max_dist = intraPeriod;
		//Reconfig
		if (vpx_codec_enc_config_set(&encoder,&config)!=VPX_CODEC_OK)
			//Exit
			return Error("-Reconfig error\n");
		//Set max data rate for Intra frames.
		//	This value controls additional clamping on the maximum size of a keyframe.
		//	It is expressed as a percentage of the average per-frame bitrate, with the
		//	special (and default) value 0 meaning unlimited, or no additional clamping
		//	beyond the codec's built-in algorithm.
		//	For example, to allocate no more than 4.5 frames worth of bitrate to a keyframe, set this to 450.
		vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT,0);
	}
	
	return 1;
}


int VP8Encoder::OpenCodec()
{
	Log("-OpenCodec VP8 using %s [%dkbps,%dfps,%dintra]\n",vpx_codec_iface_name(interface),bitrate,fps,intraPeriod);

	// Check
	if (opened)
		return Error("Codec already opened\n");
	
	// populate encoder configuration with default values
	if (vpx_codec_enc_config_default(interface, &config, 0))
		return Error("");

	//Create image
	pic = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, width, height, 0);

	//Set parameters
	config.g_profile = 1;
	config.g_w = width;
	config.g_h = height;
	config.rc_target_bitrate = bitrate;
	config.g_timebase.num = 1;
	config.g_timebase.den = 1000;
	config.g_error_resilient = VPX_ERROR_RESILIENT_PARTITIONS;  /**< The frame partitions are
									 independently decodable by the
									 bool decoder, meaning that
									 partitions can be decoded even
									 though earlier partitions have
									 been lost. Note that intra
									 predicition is still done over
									 the partition boundary. */
	config.g_lag_in_frames = 0; // 0- no frame lagging
	config.g_threads = 1;
	// rate control settings
	config.rc_dropframe_thresh = 0;
	config.rc_end_usage = VPX_CBR;
	config.g_pass = VPX_RC_ONE_PASS;
	config.kf_mode = VPX_KF_DISABLED;
	config.kf_min_dist = intraPeriod;
	config.rc_resize_allowed = 0;
	config.rc_min_quantizer = 2;
	config.rc_max_quantizer = 56;
	//Rate control adaptation undershoot control.
	//	This value, expressed as a percentage of the target bitrate, 
	//	controls the maximum allowed adaptation speed of the codec.
	//	This factor controls the maximum amount of bits that can be
	//	subtracted from the target bitrate in order to compensate for
	//	prior overshoot.
	//	Valid values in the range 0-1000.
	config.rc_undershoot_pct = 100;
	//Rate control adaptation overshoot control.
	//	This value, expressed as a percentage of the target bitrate, 
	//	controls the maximum allowed adaptation speed of the codec.
	//	This factor controls the maximum amount of bits that can be
	//	added to the target bitrate in order to compensate for prior
	//	undershoot.
	//	Valid values in the range 0-1000.
	config.rc_overshoot_pct = 15;
	//Decoder Buffer Size.
	//	This value indicates the amount of data that may be buffered
	//	by the decoding application. Note that this value is expressed
	//	in units of time (milliseconds). For example, a value of 5000
	//	indicates that the client will buffer (at least) 5000ms worth
	//	of encoded data. Use the target bitrate (rc_target_bitrate) to
	//	convert to bits/bytes, if necessary.
	config.rc_buf_sz = 1000;
	//Decoder Buffer Initial Size.
	//	This value indicates the amount of data that will be buffered 
	//	by the decoding application prior to beginning playback.
	//	This value is expressed in units of time (milliseconds).
	//	Use the target bitrate (rc_target_bitrate) to convert to
	//	bits/bytes, if necessary.
	config.rc_buf_initial_sz = 500;
	//Decoder Buffer Optimal Size.
	//	This value indicates the amount of data that the encoder should
	//	try to maintain in the decoder's buffer. This value is expressed
	//	in units of time (milliseconds).
	//	Use the target bitrate (rc_target_bitrate) to convert to
	//	bits/bytes, if necessary.
	config.rc_buf_optimal_sz = 600;
	
	//Check result
	if (vpx_codec_enc_init(&encoder, interface, &config, VPX_CODEC_USE_OUTPUT_PARTITION)!=VPX_CODEC_OK)
		//Error
		return Error("WEBRTC_VIDEO_CODEC_UNINITIALIZED [error %d:%s]\n",encoder.err,encoder.err_detail);

	//The static threshold imposes a change threshold on blocks below which they will be skipped by the encoder.
	vpx_codec_control(&encoder, VP8E_SET_STATIC_THRESHOLD, 100);
	//Set cpu usage, a bit lower than normal (-6) but higher than android (-12)
	vpx_codec_control(&encoder, VP8E_SET_CPUUSED, -8);
	//Only one partition
	vpx_codec_control(&encoder, VP8E_SET_TOKEN_PARTITIONS,VP8_ONE_TOKENPARTITION);
	//Enable noise reduction
	vpx_codec_control(&encoder, VP8E_SET_NOISE_SENSITIVITY,0);
	//Set max data rate for Intra frames.
	//	This value controls additional clamping on the maximum size of a keyframe.
	//	It is expressed as a percentage of the average per-frame bitrate, with the
	//	special (and default) value 0 meaning unlimited, or no additional clamping
	//	beyond the codec's built-in algorithm.
	//	For example, to allocate no more than 4.5 frames worth of bitrate to a keyframe, set this to 450.
	vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT,0);

	// We are opened
	opened=true;

	// Exit
	return 1;
}

int VP8Encoder::FastPictureUpdate()
{
	 forceKeyFrame = true;
}

VideoFrame* VP8Encoder::EncodeFrame(BYTE *buffer,DWORD bufferSize)
{
	if(!opened)
	{
		Error("-Codec not opened\n");
		return NULL;
	}

	//Comprobamos el tamaño
	if (numPixels*3/2 != bufferSize)
	{
		Error("-EncodeFrame length error [%d,%d]\n",numPixels*5/4,bufferSize);
		return NULL;
	}
	
	//Set data
	pic->planes[VPX_PLANE_Y] = buffer;
	pic->planes[VPX_PLANE_U] = buffer+numPixels;
	pic->planes[VPX_PLANE_V] = buffer+numPixels*5/4;

	int flags = 0;

	//Check FPU
	if (forceKeyFrame)
	{
		//Set flag
		flags = VPX_EFLAG_FORCE_KF;
		//Not next one
		forceKeyFrame = false;
	}
		
	uint32_t duration = 1000 / fps;

	if (vpx_codec_encode(&encoder, pic, pts, duration, flags, VPX_DL_REALTIME)!=VPX_CODEC_OK)
	{
		//Error
		Error("WEBRTC_VIDEO_CODEC_ERROR [error %d:%s]\n",encoder.err,encoder.err_detail);
		//Exit
		return NULL;
	}

	//Increase timestamp
	pts += duration;

	vpx_codec_iter_t iter = NULL;
	int partitionIndex = 0;
	const vpx_codec_cx_pkt_t *pkt = NULL;

	//Check size
	if (!frame)
		//Create new frame
		frame = new VideoFrame(type,262143);

	//Set width and height
	frame->SetWidth(width);
	frame->SetHeight(height);
	
	//Emtpy rtp info
	frame->ClearRTPPacketizationInfo();

	//Emtpy
	frame->SetLength(0);

	//For each packet
	while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)) != NULL)
	{
		//Set intra
		frame->SetIntra(pkt->data.frame.flags & VPX_FRAME_IS_KEY);

		if (pkt->kind==VPX_CODEC_CX_FRAME_PKT)
		{
			VP8PayloadDescriptor desc;
			//Append data to the frame
			DWORD pos = frame->AppendMedia((BYTE*)pkt->data.frame.buf,pkt->data.frame.sz);
			//Set data
			desc.extendedControlBitsPresent = 0;
			desc.nonReferencePicture = pkt->data.frame.flags & VPX_FRAME_IS_DROPPABLE;
			desc.startOfPartition	 = true;
			desc.partitionIndex	 = partitionIndex;
			//Split into MTU
			DWORD cur = 0;
			//For each
			while (cur<pkt->data.frame.sz)
			{
				//Serialized desc
				BYTE aux[6];
				//Serialize
				DWORD auxLen = desc.Serialize(aux,6);
				//Get
				DWORD len = RTPPAYLOADSIZE-desc.GetSize();
				//Check iw we have enought
				if (cur+len>pkt->data.frame.sz)
					//Reduce
					len = pkt->data.frame.sz-cur;
				//Append hint
				frame->AddRtpPacket(pos+cur,len,aux,auxLen);
				//Increase current
				cur+=len;
				//Not first in partition
				desc.startOfPartition = false;
			}
			//Next partition
			partitionIndex++;
		}
	}

	return frame;
}
