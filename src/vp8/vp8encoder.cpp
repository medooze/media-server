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


VP8Encoder::VP8Encoder(const Properties& properties) : frame(VideoCodec::VP8)
{
	// Set default values
	type    = VideoCodec::VP8;

	for (Properties::const_iterator it = properties.begin(); it != properties.end(); ++it)
		Debug("-VP8Encoder::VP8Encoder() | Setting property [%s:%s]\n", it->first.c_str(), it->second.c_str());

	threads				= properties.GetProperty("vp8.threads"			, 1);
	cpuused				= properties.GetProperty("vp8.cpuused"			, -4);
	maxKeyFrameBitratePct		= properties.GetProperty("vp8.max_keyframe_bitrate_pct"	, 0); // 0 means no limitation
	deadline			= properties.GetProperty("vp8.deadline"			, VPX_DL_REALTIME);
	endUsage			= (vpx_rc_mode)properties.GetProperty("vp8.rc_end_usage", VPX_CBR);
	dropFrameThreshold		= properties.GetProperty("vp8.rc_dropframe_thresh"	, 0);
	minQuantizer			= properties.GetProperty("vp8.rc_min_quantizer"		, 2);
	maxQuantizer			= properties.GetProperty("vp8.rc_max_quantizer"		, 56);
	undershootPct			= properties.GetProperty("vp8.rc_undershoot_pct"	, 100);
	overshootPct			= properties.GetProperty("vp8.rc_overshoot_pct"		, 15);
	bufferSize			= properties.GetProperty("vp8.rc_buf_sz"		, 1000);
	bufferInitialSize		= properties.GetProperty("vp8.rc_buf_initial_sz"	, 500);
	bufferOptimalSize		= properties.GetProperty("vp8.rc_buf_optimal_sz"	, 600);
	staticThreshold			= properties.GetProperty("vp8.static_thresh"		, 100);
	noiseReductionSensitivity	= properties.GetProperty("vp8.noise_sensitivity"	, 0);

	//Disable sharing buffer on clone
	frame.DisableSharedBuffer();

	//Add default codec configuration
	VP8CodecConfig vp8Config;
	//Allocate config
	frame.AllocateCodecConfig(vp8Config.GetSize());
	//Add codec config
	vp8Config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
	
}

VP8Encoder::~VP8Encoder()
{
	vpx_codec_destroy(&encoder);
	if (pic)
		vpx_img_free(pic);
}

/**********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
***********************/
int VP8Encoder::SetSize(int width, int height)
{
	//Check if they are the same size
	if (this->width == width && this->height == height)
		//Do nothing
		return 1;

	Log("-VP8Encoder::SetSize() [width:%d,height:%d]\n", width, height);

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
	this->intraPeriod = intraPeriod;

	//Check if already opened
	if (opened)
	{
		//Reconfig parameters
		UltraDebug("VP8Encoder::SetFrameRate() | Reset codec config with bitrate: %d, frames: %d, max_keyframe_bitrate_pct: %d\n", bitrate, frames, maxKeyFrameBitratePct);
		config.rc_target_bitrate = bitrate;
		config.kf_mode = intraPeriod ? VPX_KF_AUTO : VPX_KF_DISABLED;
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
		vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT, maxKeyFrameBitratePct);
	}
	
	return 1;
}


int VP8Encoder::OpenCodec()
{
	Log("-VP8Encoder::OpenCodec() | VP8 using %s [%dkbps,%dfps,%dintra,cpuused:%d,threads:%d,max_keyframe_bitrate_pct:%d,deadline:%d]\n",vpx_codec_iface_name(interface),bitrate,fps,intraPeriod,cpuused,threads,maxKeyFrameBitratePct,deadline);

	// Check
	if (opened)
		return Error("Codec already opened\n");
	
	// populate encoder configuration with default values
	if (vpx_codec_enc_config_default(interface, &config, 0))
		return Error("-VP8Encoder::OpenCodec() | Error setting config defaults\n");

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
	config.g_threads = threads;
	// rate control settings
	config.rc_dropframe_thresh = 0;
	config.rc_end_usage = endUsage;
	config.g_pass = VPX_RC_ONE_PASS;
	config.kf_mode = intraPeriod ? VPX_KF_AUTO : VPX_KF_DISABLED;
	config.kf_min_dist = 0;
	config.kf_max_dist = intraPeriod;
	config.rc_resize_allowed = 0;
	config.rc_min_quantizer = minQuantizer;
	config.rc_max_quantizer = maxQuantizer;
	//Rate control adaptation undershoot control.
	//	This value, expressed as a percentage of the target bitrate, 
	//	controls the maximum allowed adaptation speed of the codec.
	//	This factor controls the maximum amount of bits that can be
	//	subtracted from the target bitrate in order to compensate for
	//	prior overshoot.
	//	Valid values in the range 0-1000.
	config.rc_undershoot_pct = undershootPct;
	//Rate control adaptation overshoot control.
	//	This value, expressed as a percentage of the target bitrate, 
	//	controls the maximum allowed adaptation speed of the codec.
	//	This factor controls the maximum amount of bits that can be
	//	added to the target bitrate in order to compensate for prior
	//	undershoot.
	//	Valid values in the range 0-1000.
	config.rc_overshoot_pct = overshootPct;
	//Decoder Buffer Size.
	//	This value indicates the amount of data that may be buffered
	//	by the decoding application. Note that this value is expressed
	//	in units of time (milliseconds). For example, a value of 5000
	//	indicates that the client will buffer (at least) 5000ms worth
	//	of encoded data. Use the target bitrate (rc_target_bitrate) to
	//	convert to bits/bytes, if necessary.
	config.rc_buf_sz = bufferSize;
	//Decoder Buffer Initial Size.
	//	This value indicates the amount of data that will be buffered 
	//	by the decoding application prior to beginning playback.
	//	This value is expressed in units of time (milliseconds).
	//	Use the target bitrate (rc_target_bitrate) to convert to
	//	bits/bytes, if necessary.
	config.rc_buf_initial_sz = bufferInitialSize;
	//Decoder Buffer Optimal Size.
	//	This value indicates the amount of data that the encoder should
	//	try to maintain in the decoder's buffer. This value is expressed
	//	in units of time (milliseconds).
	//	Use the target bitrate (rc_target_bitrate) to convert to
	//	bits/bytes, if necessary.
	config.rc_buf_optimal_sz = bufferOptimalSize;
	
	//Check result
	if (vpx_codec_enc_init(&encoder, interface, &config, VPX_CODEC_USE_OUTPUT_PARTITION)!=VPX_CODEC_OK)
		//Error
		return Error("-VP8Encoder::OpenCodec() | Error initializing encoder [error %d:%s]\n",encoder.err,encoder.err_detail);

	//The static threshold imposes a change threshold on blocks below which they will be skipped by the encoder.
	vpx_codec_control(&encoder, VP8E_SET_STATIC_THRESHOLD, staticThreshold);
	//Set cpu usage
	vpx_codec_control(&encoder, VP8E_SET_CPUUSED, cpuused);
	//Only one partition
	vpx_codec_control(&encoder, VP8E_SET_TOKEN_PARTITIONS, VP8_ONE_TOKENPARTITION);
	//Enable noise reduction
	vpx_codec_control(&encoder, VP8E_SET_NOISE_SENSITIVITY, noiseReductionSensitivity);
	//Set max data rate for Intra frames.
	//	This value controls additional clamping on the maximum size of a keyframe.
	//	It is expressed as a percentage of the average per-frame bitrate, with the
	//	special (and default) value 0 meaning unlimited, or no additional clamping
	//	beyond the codec's built-in algorithm.
	//	For example, to allocate no more than 4.5 frames worth of bitrate to a keyframe, set this to 450.
	vpx_codec_control(&encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT, maxKeyFrameBitratePct);

	// We are opened
	opened=true;

	// Exit
	return 1;
}

int VP8Encoder::FastPictureUpdate()
{
	forceKeyFrame = true;
	return true;
}

VideoFrame* VP8Encoder::EncodeFrame(const VideoBuffer::const_shared& videoBuffer)
{
	if(!opened)
	{
		Error("-VP8Encoder::EncodeFrame() | Codec not opened\n");
		return NULL;
	}

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

	if (videoBuffer)
	{
		//Get planes
		const Plane& y = videoBuffer->GetPlaneY();
		const Plane& u = videoBuffer->GetPlaneU();
		const Plane& v = videoBuffer->GetPlaneV();

		//Set data
		pic->planes[VPX_PLANE_Y] = (unsigned char*)y.GetData();
		pic->planes[VPX_PLANE_U] = (unsigned char*)u.GetData();
		pic->planes[VPX_PLANE_V] = (unsigned char*)v.GetData();
		pic->planes[VPX_PLANE_ALPHA] = nullptr;
		pic->stride[VPX_PLANE_Y] = y.GetStride();
		pic->stride[VPX_PLANE_U] = u.GetStride();
		pic->stride[VPX_PLANE_V] = v.GetStride();
		pic->stride[VPX_PLANE_ALPHA] = 0;

		//Set color range
		switch (videoBuffer->GetColorRange())
		{
			case VideoBuffer::ColorRange::Partial:
				pic->range = VPX_CR_STUDIO_RANGE;
				break;
			case VideoBuffer::ColorRange::Full:
				pic->range = VPX_CR_FULL_RANGE;
				break;
			default:
				pic->range = VPX_CR_FULL_RANGE;
		}

		//Get color space
		switch (videoBuffer->GetColorSpace())
		{
			case VideoBuffer::ColorSpace::SRGB:
				pic->cs = VPX_CS_SRGB;
				break;
			case VideoBuffer::ColorSpace::BT709:
				pic->cs = VPX_CS_BT_709;
				break;
			case VideoBuffer::ColorSpace::BT601:
				pic->cs = VPX_CS_BT_601;
				break;
			case VideoBuffer::ColorSpace::SMPTE170:
				pic->cs = VPX_CS_SMPTE_170;
				break;
			case VideoBuffer::ColorSpace::SMPTE240:
				pic->cs = VPX_CS_SMPTE_240;
				break;
			case VideoBuffer::ColorSpace::BT2020:
				pic->cs = VPX_CS_BT_2020;
				break;
			default:
				//Unknown
				pic->cs = VPX_CS_UNKNOWN;
		}

		if (vpx_codec_encode(&encoder, pic, pts, duration, flags, deadline)!=VPX_CODEC_OK)
		{
			//Error
			Error("-VP8Encoder::EncodeFrame() | Encode error [error %d:%s]\n",encoder.err,encoder.err_detail);
			//Exit
			return nullptr;
		}
	}
	// end of stream, flushing buffered encoded frame
	else
	{
		if (vpx_codec_encode(&encoder, nullptr, pts, duration, flags, deadline)!=VPX_CODEC_OK)
		{
			//Error
			Error("-VP8Encoder::EncodeFrame() | Encode error [error %d:%s]\n",encoder.err,encoder.err_detail);
			//Exit
			return nullptr;
		}
	}

	//Increase timestamp
	pts += duration;

	vpx_codec_iter_t iter = NULL;
	int partitionIndex = 0;
	const vpx_codec_cx_pkt_t *pkt = NULL;

	//Set width and height
	frame.SetWidth(width);
	frame.SetHeight(height);
	
	//Emtpy rtp info
	frame.ClearRTPPacketizationInfo();

	//Emtpy
	frame.SetLength(0);

	//For each packet
	while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)) != NULL)
	{
		//Set intra
		frame.SetIntra(pkt->data.frame.flags & VPX_FRAME_IS_KEY);
	
		if (pkt->kind==VPX_CODEC_CX_FRAME_PKT)
		{
			VP8PayloadDescriptor desc;
			//Append data to the frame
			DWORD pos = frame.AppendMedia((BYTE*)pkt->data.frame.buf,pkt->data.frame.sz);
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
				frame.AddRtpPacket(pos+cur,len,aux,auxLen);
				//Increase current
				cur+=len;
				//Not first in partition
				desc.startOfPartition = false;
			}
			//Next partition
			partitionIndex++;
		}
	}

	return &frame;
}
