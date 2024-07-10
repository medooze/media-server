#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tgmath.h>
#include "log.h"
#include "av1/AV1.h"
#include "av1/Obu.h"
#include "av1/AV1Encoder.h"
#include "av1/AV1CodecConfigurationRecord.h"

#define SET_ENCODER_PARAM_OR_RETURN_ERROR(id, value) \
  do { \
     aom_codec_err_t err = aom_codec_control(&encoder, id, value); \
     if (err != AOM_CODEC_OK) \
	    Error("-AV1Encoder::SetEncoderControlParameters() [id:%d,value%d,err:%d]\n", id, value, err); \
	 else \
		Debug("-AV1Encoder::SetEncoderControlParameters() [id:%d,value%d,err:%d]\n", id, value, err); \
  } while (0);

aom_superblock_size_t GetSuperblockSize(int width, int height, int threads) {
	int resolution = width * height;
	if (threads >= 4 && resolution >= 960 * 540 && resolution < 1920 * 1080)
		return AOM_SUPERBLOCK_SIZE_64X64;
	else
		return AOM_SUPERBLOCK_SIZE_DYNAMIC;
}

AV1Encoder::AV1Encoder(const Properties& properties) : frame(VideoCodec::AV1)
{
	// Set default values
	type = VideoCodec::AV1;

	for (Properties::const_iterator it = properties.begin(); it != properties.end(); ++it)
		Debug("-AV1Encoder::AV1Encoder() | Setting property [%s:%s]\n", it->first.c_str(), it->second.c_str());

	threads = properties.GetProperty("av1.threads", 1);
	cpuused = properties.GetProperty("av1.cpuused", 10);
	maxKeyFrameBitratePct = properties.GetProperty("av1.max_keyframe_bitrate_pct", 300); // 0 means no limitation
	endUsage = (aom_rc_mode)properties.GetProperty("av1.rc_end_usage", AOM_CBR);
	dropFrameThreshold = properties.GetProperty("av1.rc_dropframe_thresh", 0);
	minQuantizer = properties.GetProperty("av1.rc_min_quantizer", 2);
	maxQuantizer = properties.GetProperty("av1.rc_max_quantizer", 63);
	undershootPct = properties.GetProperty("av1.rc_undershoot_pct", 50);
	overshootPct = properties.GetProperty("av1.rc_overshoot_pct", 50);
	bufferSize = properties.GetProperty("av1.rc_buf_sz", 1000);
	bufferInitialSize = properties.GetProperty("av1.rc_buf_initial_sz", 600);
	bufferOptimalSize = properties.GetProperty("av1.rc_buf_optimal_sz", 600);
	noiseReductionSensitivity = properties.GetProperty("av1.noise_sensitivity", 0);
	aqMode = properties.GetProperty("av1.aq_mode", 3);

	//Disable sharing buffer on clone
	frame.DisableSharedBuffer();

	//Add default codec configuration
	AV1CodecConfig AV1Config;
	//Allocate config
	frame.AllocateCodecConfig(AV1Config.GetSize());
	//Add codec config
	AV1Config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());

}

AV1Encoder::~AV1Encoder()
{
	aom_codec_destroy(&encoder);
	if (pic)
		aom_img_free(pic);
}

/**********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
***********************/
int AV1Encoder::SetSize(int width, int height)
{
	//Check if they are the same size
	if (this->width == width && this->height == height)
		//Do nothing
		return 1;

	Log("-AV1Encoder::SetSize() [width:%d,height:%d]\n", width, height);

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
int AV1Encoder::SetFrameRate(int frames, int kbits, int intraPeriod)
{
	// Save frame rate
	if (frames > 0)
		fps = frames;
	else
		fps = 10;

	// Save bitrate
	if (kbits > 0)
		bitrate = kbits;

	//Save intra period
	this->intraPeriod = intraPeriod;

	//Check if already opened
	if (opened)
	{
		//Reconfig parameters
		UltraDebug("AV1Encoder::SetFrameRate() | Reset codec config with bitrate: %d, frames: %d, max_keyframe_bitrate_pct: %d\n", bitrate, frames, maxKeyFrameBitratePct);
		config.rc_target_bitrate = bitrate;
		config.kf_mode = intraPeriod ? AOM_KF_AUTO : AOM_KF_DISABLED;
		config.kf_max_dist = intraPeriod;
		//Reconfig
		if (aom_codec_enc_config_set(&encoder, &config) != AOM_CODEC_OK)
			//Exit
			return Error("-Reconfig error\n");
		//Set max data rate for Intra frames.
		//	This value controls additional clamping on the maximum size of a keyframe.
		//	It is expressed as a percentage of the average per-frame bitrate, with the
		//	special (and default) value 0 meaning unlimited, or no additional clamping
		//	beyond the codec's built-in algorithm.
		//	For example, to allocate no more than 4.5 frames worth of bitrate to a keyframe, set this to 450.
		aom_codec_control(&encoder, AOME_SET_MAX_INTRA_BITRATE_PCT, maxKeyFrameBitratePct);
	}

	return 1;
}


int AV1Encoder::OpenCodec()
{
	Log(">AV1Encoder::OpenCodec() | AV1 using %s [%dkbps,%dfps,%dintra,cpuused:%d,threads:%d,max_keyframe_bitrate_pct:%d,deadline:%d]\n", aom_codec_iface_name(aom_codec_av1_cx()), bitrate, fps, intraPeriod, cpuused, threads, maxKeyFrameBitratePct, deadline);

	// Check
	if (opened)
		return Error("-AV1Encoder::OpenCodec() | Codec already opened\n");

	// populate encoder configuration with default values
	if (aom_codec_enc_config_default(aom_codec_av1_cx(), &config, 0))
		return Error("-AV1Encoder::OpenCodec() | Error setting config defaults\n");


	//Create image
	pic = aom_img_alloc(NULL, AOM_IMG_FMT_I420, width, height, 0);

	//Set parameters
	config.g_profile = 0;
	config.g_input_bit_depth = 8;

	config.g_w = width;
	config.g_h = height;
	config.g_threads = threads;
	config.g_timebase.num = 1;
	config.g_timebase.den = 1000;
	config.rc_target_bitrate = bitrate;

	config.g_error_resilient = 0;
	config.g_lag_in_frames = 0; // 0- no frame lagging
	
	// rate control settings
	config.rc_dropframe_thresh = 0;
	config.rc_end_usage = endUsage;
	config.g_pass = AOM_RC_ONE_PASS;
	config.kf_mode = intraPeriod ? AOM_KF_AUTO : AOM_KF_DISABLED;
	config.kf_min_dist = 0;
	config.kf_max_dist = intraPeriod;
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

	config.g_usage = AOM_USAGE_REALTIME;

	//Check result
	if (aom_codec_enc_init(&encoder, aom_codec_av1_cx(), &config, 0) != AOM_CODEC_OK)
		//Error
		return Error("-AV1Encoder::OpenCodec() | Error initializing encoder [error %d:%s]\n", encoder.err, encoder.err_detail);

	// Set control parameters
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AOME_SET_CPUUSED, cpuused);
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_CDEF, 1);
#ifdef AV1E_SET_ENABLE_TPL_MODEL
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_TPL_MODEL, 0);
#endif
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_DELTAQ_MODE, 0);
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_ORDER_HINT, 0);
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_AQ_MODE, 3);
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AOME_SET_MAX_INTRA_BITRATE_PCT, maxKeyFrameBitratePct);
#ifdef AV1E_SET_COEFF_COST_UPD_FREQ
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_COEFF_COST_UPD_FREQ, 3);
#endif
#ifdef AV1E_SET_MODE_COST_UPD_FREQ
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_MODE_COST_UPD_FREQ, 3);
#endif
#ifdef AV1E_SET_MV_COST_UPD_FREQ
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_MV_COST_UPD_FREQ, 3);
#endif
#ifdef AV1E_SET_ENABLE_PALETTE
	//Not screenshare
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_PALETTE, 0);
#endif

	if (config.g_threads == 8) {
		// Values passed to AV1E_SET_TILE_ROWS and AV1E_SET_TILE_COLUMNS are log2()
		// based.
		// Use 4 tile columns x 2 tile rows for 8 threads.
		SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_TILE_ROWS, 1);
		SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_TILE_COLUMNS, 2);
	}
	else if (config.g_threads == 4) {
		// Use 2 tile columns x 2 tile rows for 4 threads.
		SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_TILE_ROWS, 1);
		SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_TILE_COLUMNS, 1);
	}
	else {
		SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_TILE_COLUMNS,
			static_cast<int>(log2(config.g_threads)));
	}

#ifdef AV1E_SET_ROW_MT
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ROW_MT, 1);
#endif 
#ifdef AV1E_SET_ENABLE_OBMC
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_OBMC, 0);
#endif
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_NOISE_SENSITIVITY, noiseReductionSensitivity);
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_WARPED_MOTION, 0);
#ifdef AV1E_SET_ENABLE_GLOBAL_MOTION
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
#endif
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_REF_FRAME_MVS, 0);
	SET_ENCODER_PARAM_OR_RETURN_ERROR(
		AV1E_SET_SUPERBLOCK_SIZE,
		GetSuperblockSize(config.g_w, config.g_h, config.g_threads));
#ifdef AV1E_SET_ENABLE_CFL_INTRA
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_CFL_INTRA, 0);
#endif
#ifdef AV1E_SET_ENABLE_SMOOTH_INTRA
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_SMOOTH_INTRA, 0);
#endif
#ifdef AV1E_SET_ENABLE_ANGLE_DELTA
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_ANGLE_DELTA, 0);
#endif
#ifdef AV1E_SET_ENABLE_FILTER_INTRA
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_FILTER_INTRA, 0);
#endif
#ifdef AV1E_SET_INTRA_DEFAULT_TX_ONLY
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_INTRA_DEFAULT_TX_ONLY, 1);
#endif
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_DISABLE_TRELLIS_QUANT, 1);
#ifdef AV1E_SET_ENABLE_DIST_WTD_COMP
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_DIST_WTD_COMP, 0);
#endif
#ifdef AV1E_SET_ENABLE_DIFF_WTD_COMP
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_DIFF_WTD_COMP, 0);
#endif
#ifdef AV1E_SET_ENABLE_DUAL_FILTER
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_DUAL_FILTER, 0);
#endif
#ifdef AV1E_SET_ENABLE_INTERINTRA_COMP
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_INTERINTRA_COMP, 0);
#endif
#ifdef AV1E_SET_ENABLE_INTERINTRA_WEDGE
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_INTERINTRA_WEDGE, 0);
#endif
#ifdef AV1E_SET_ENABLE_INTRA_EDGE_FILTER
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_INTRA_EDGE_FILTER, 0);
#endif
#ifdef AV1E_SET_ENABLE_INTRABC
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_INTRABC, 0);
#endif
#ifdef AV1E_SET_ENABLE_MASKED_COMP
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_MASKED_COMP, 0);
#endif
#ifdef AV1E_SET_ENABLE_PAETH_INTRA
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_PAETH_INTRA, 0);
#endif
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_QM, 0);
#ifdef AV1E_SET_ENABLE_RECT_PARTITIONS
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_RECT_PARTITIONS, 0);
#endif
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_RESTORATION, 0);
#ifdef AV1E_SET_ENABLE_SMOOTH_INTERINTRA
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_SMOOTH_INTERINTRA, 0);
#endif
#ifdef AV1E_SET_ENABLE_TX64
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_ENABLE_TX64, 0);
#endif
#ifdef AV1E_SET_MAX_REFERENCE_FRAMES
	SET_ENCODER_PARAM_OR_RETURN_ERROR(AV1E_SET_MAX_REFERENCE_FRAMES, 3);
#endif

	// We are opened
	opened = true;

	Log("<AV1Encoder::OpenCodec()\n [opened:%d]\n", opened);

	// Exit
	return 1;
}

int AV1Encoder::FastPictureUpdate()
{
	forceKeyFrame = true;
	return true;
}

VideoFrame* AV1Encoder::EncodeFrame(const VideoBuffer::const_shared& videoBuffer)
{
	Debug(">AV1Encoder::EncodeFrame()\n");

	if (!opened)
	{
		Error("-AV1Encoder::EncodeFrame() | Codec not opened\n");
		return NULL;
	}

	int flags = 0;

	//Check FPU
	if (forceKeyFrame)
	{
		//Set flag
		flags = AOM_EFLAG_FORCE_KF;
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
		pic->planes[AOM_PLANE_Y] = (unsigned char*)y.GetData();
		pic->planes[AOM_PLANE_U] = (unsigned char*)u.GetData();
		pic->planes[AOM_PLANE_V] = (unsigned char*)v.GetData();
		pic->stride[AOM_PLANE_Y] = y.GetStride();
		pic->stride[AOM_PLANE_U] = u.GetStride();
		pic->stride[AOM_PLANE_V] = v.GetStride();

		/*
		//Set color range
		switch (videoBuffer->GetColorRange())
		{
			case VideoBuffer::ColorRange::Partial:
				pic->range = AOM_CR_STUDIO_RANGE;
				break;
			case VideoBuffer::ColorRange::Full:
				pic->range = AOM_CR_FULL_RANGE;
				break;
			default:
				pic->range = AOM_CR_FULL_RANGE;
		}

		//Get color space
		switch (videoBuffer->GetColorSpace())
		{
			case VideoBuffer::ColorSpace::SRGB:
				pic->cs = AOM_CS_SRGB;
				break;
			case VideoBuffer::ColorSpace::BT709:
				pic->cs = AOM_CS_BT_709;
				break;
			case VideoBuffer::ColorSpace::BT601:
				pic->cs = AOM_CS_BT_601;
				break;
			case VideoBuffer::ColorSpace::SMPTE170:
				pic->cs = AOM_CS_SMPTE_170;
				break;
			case VideoBuffer::ColorSpace::SMPTE240:
				pic->cs = AOM_CS_SMPTE_240;
				break;
			case VideoBuffer::ColorSpace::BT2020:
				pic->cs = AOM_CS_BT_2020;
				break;
			default:
				//Unknown
				pic->cs = AOM_CS_UNKNOWN;
		}*/

		if (aom_codec_encode(&encoder, pic, pts, duration, flags) != AOM_CODEC_OK)
		{
			//Error
			Error("-AV1Encoder::EncodeFrame() | Encode error [error %d:%s]\n", encoder.err, encoder.err_detail);
			//Exit
			return nullptr;
		}
	}
	// end of stream, flushing buffered encoded frame
	else
	{
		if (aom_codec_encode(&encoder, nullptr, pts, duration, flags) != AOM_CODEC_OK)
		{
			//Error
			Error("-AV1Encoder::EncodeFrame() | Encode error [error %d:%s]\n", encoder.err, encoder.err_detail);
			//Exit
			return nullptr;
		}
	}

	//Increase timestamp
	pts += duration;

	aom_codec_iter_t iter = NULL;
	const aom_codec_cx_pkt_t* pkt = NULL;

	//Set width and height
	frame.SetWidth(width);
	frame.SetHeight(height);

	//Emtpy rtp info
	frame.ClearRTPPacketizationInfo();

	//Emtpy
	frame.SetLength(0);

	//For each packet
	while ((pkt = aom_codec_get_cx_data(&encoder, &iter)) != NULL)
	{
		//For each coded packet
		if (pkt->kind == AOM_CODEC_CX_FRAME_PKT)
		{
			if (pkt->data.frame.flags & AOM_FRAME_IS_KEY)
				//Set intra
				frame.SetIntra(true);

			//Copy data to frame
			auto ini = frame.AppendMedia((uint8_t*)pkt->data.frame.buf, pkt->data.frame.sz);
			//Get reader for av1 encoded packet
			BufferReader reader(*frame.GetBuffer());

			//Get initial position of reader
			auto mark = reader.Mark();

			//For each obu
			ObuHeader obuHeader;
			RtpAv1AggreationHeader header;
			while (reader.GetLeft() && obuHeader.Parse(reader))
			{
				//Get length from header or read the rest available
				auto payloadSize = obuHeader.length.value_or(reader.GetLeft());
				//Skip header and size from media data
				auto pos = ini + reader.GetOffset(mark);

				//UltraDebug("-AV1Encoder::EncodeFrame() [left:%llu,payloadSize:%llu, pos:%llu]\n", reader.GetLeft(), payloadSize, pos);

				//Ensure we have enought data for the rest of the obu
				if (!reader.Assert(payloadSize))
					break;

				//Skip the rest of the obu
				reader.Skip(payloadSize);

				//We are not going to to write the length of the obu framgent
				obuHeader.length.reset();

				//If we need to fragmentize into multiple rtp packets
				if ((payloadSize + header.GetSize() + obuHeader.GetSize()) > RTPPAYLOADSIZE)
				{
					bool firstSegment = true;

					while (payloadSize)
					{
						//Calculate fragment size
						auto fragSize = std::min(payloadSize, size_t(RTPPAYLOADSIZE - header.GetSize() - obuHeader.GetSize()));

						//RTP aggregation header for only 1 OBU segment
						header.field.W = 1;										 // Send one OBU element each time
						header.field.Z = !firstSegment;							 // Has prev fragment
						header.field.Y = (fragSize == payloadSize) ? 0 : 1;		 // Has next fragment
						header.field.N = frame.GetRtpPacketizationInfo().empty() && frame.IsIntra() ? 1 : 0; // First packet of new coded sequence

						//RTP preffix with max possible size
						Buffer preffix(header.GetSize() + obuHeader.GetSize());
						BufferWritter writter(preffix);

						//Write fragmentation header 
						header.Serialize(writter);

						if (firstSegment)
						{
							//Write OBU header on first segment
							obuHeader.Serialize(writter);
							//Next is not first
							firstSegment = false;
						}

						//Add rtp packet
						frame.AddRtpPacket(pos, fragSize, writter.GetData(), writter.GetLength());
						//Next fragment
						pos += fragSize;
						//Remove fragment size from total
						payloadSize -= fragSize;
					}
				}
				else
				{
					//RTP aggregation header for only 1 OBU segment
					header.field.W = 1; // Send one OBU element each time
					header.field.Z = 0; // No prev fragment
					header.field.Y = 0; // No next fragment
					header.field.N = frame.GetRtpPacketizationInfo().empty() && frame.IsIntra() ? 1 : 0; // First packet of new coded sequence

					//RTP preffix
					Buffer preffix(header.GetSize() + obuHeader.GetSize());
					BufferWritter writter(preffix);

					//Write fragmentation header 
					header.Serialize(writter);
					//Write OBU header
					obuHeader.Serialize(writter);
					//Add rtp packet
					frame.AddRtpPacket(pos, payloadSize, writter.GetData(), writter.GetLength());
				}
			}
		}
	}

	Debug("<AV1Encoder::EncodeFrame()\n");

	return &frame;
}


