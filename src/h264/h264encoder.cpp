#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "log.h"
#include "h264encoder.h"


//////////////////////////////////////////////////////////////////////////
//Encoder
// 	Codificador H264
//
//////////////////////////////////////////////////////////////////////////

static void X264_log(void *p, int level, const char *fmt, va_list args)
{
	static char line[1024];
	//Create line
	vsnprintf(line,1024,fmt,args);
	//Remove underflow
	if (strstr(line,"VBV underflow")!=NULL)
		//Not show it
		return;
	//Print it
	Log("x264 %s",line);
}

void GetAndCheckX264HighLevelProperty(const Properties& properties, const std::string& property_name, std::optional<std::string>& var, const char * const options[], const std::string& defaultInUse)
{
	const std::string notConfiged = "";
	std::string varStr = properties.GetProperty("h264." + property_name, notConfiged);

	var.reset();
	// no property is set, leave var as nullopt
	if (varStr.empty())
	{
		return;
	}

	// this check should only apply to x264_xxx_names[]
	for (size_t i = 0; options[i]; ++i)
	{
		if (varStr == std::string(options[i]))
		{
			// found valid property, assign to var
			var.emplace(varStr);
			return;
		}
	}

	// not valid property settings
	Error("-H264Encoder::H264Encoder: Invalid %s in profile property, force it to %s\n", property_name.c_str(), defaultInUse.c_str());
}

int H264Encoder::LevelNumberToLevelIdc(const std::string& levelNumber) const
{
	// H264 A.3.2
	return (levelNumber == "1b") ? 9 : (std::stof(levelNumber) * 10);
}

/**********************
* H264Encoder
*	Constructor de la clase
***********************/
H264Encoder::H264Encoder(const Properties& properties) : frame(VideoCodec::H264)
{
	// Set default values
	type    = VideoCodec::H264;
	Debug("-H264Encoder::H264Encoder()\n");
	for (Properties::const_iterator it = properties.begin(); it != properties.end(); ++it)
		Debug("-H264Encoder::H264Encoder() | Setting property [%s:%s]\n", it->first.c_str(), it->second.c_str());

	//Number of threads or auto
	threads = properties.GetProperty("h264.threads",0);

	//Check profile/level/preset/tune
	GetAndCheckX264HighLevelProperty(properties, "profile", profile, x264_profile_names, GetProfileInUse());
	GetAndCheckX264HighLevelProperty(properties, "level", level, x264_level_names, GetLevelInUse());
	GetAndCheckX264HighLevelProperty(properties, "preset", preset, x264_preset_names, GetPresetInUse());
	GetAndCheckX264HighLevelProperty(properties, "tune", tune, x264_tune_names, GetTuneInUse());

	//ipratio
	ipratio = properties.GetProperty("h264.ipratio", -1.0F);

	//Check mode
	streaming = properties.GetProperty("h264.streaming",true);
	//ratetol, please check after h264.streaming
	ratetol = properties.GetProperty("h264.ratetol", streaming? 0.0F : 0.1F);

	//VBV buffer size in frames according to the bitrate
	bufferSizeInFrames = properties.GetProperty("h264.buffersize_in_frames", streaming ? 3 : 1);
	maxBitrateMultiplier = properties.GetProperty("h264.max_bitrate_multiplier", 1.0F);

	//Use annex b
	annexb = properties.GetProperty("h264.annexb",false);

	qMin = properties.GetProperty("h264.qmin", qMin);
	qMax = properties.GetProperty("h264.qmax", qMax);

	//Disable sharing buffer on clone
	frame.DisableSharedBuffer();
}

/**********************
* ~H264Encoder
*	Destructor
***********************/
H264Encoder::~H264Encoder()
{
	//If we have an encoder
	if (enc)
		//Close it
		x264_encoder_close(enc);
}

/**********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
***********************/
int H264Encoder::SetSize(int width, int height)
{
	//Check if they are the same size
	if (this->width==width && this->height==height)
		//Do nothing
		return 1;
	
	Log("-H264Encoder::SetSize() [width:%d,height:%d]\n",width,height);
	
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
int H264Encoder::SetFrameRate(int frames,int kbits,int intraPeriod)
{
	//UltraDebug("-H264Encoder::SetFrameRate() [fps: %d, kbits: %d, intra: %d]\n",frames,kbits,intraPeriod);
	// Save frame rate
	if (frames>0)
		fps=frames;
	else
		fps=10;

	// Save bitrate
	if (kbits>0)
		bitrate=kbits;

	//Save intra period
	this->intraPeriod = intraPeriod ? intraPeriod : X264_KEYINT_MAX_INFINITE;

	//Check if already opened
	if (opened)
	{
		//UltraDebug("-H264Encoder::SetFrameRate() | reconfig x264 encoder\n");
		//Reconfig parameters -> FPS is not allowed to be reconfigured
		params.i_keyint_max         = this->intraPeriod ;
		params.i_keyint_min	    = this->intraPeriod;
		params.rc.i_bitrate         = bitrate;
		params.rc.i_vbv_max_bitrate = bitrate * maxBitrateMultiplier;
		params.rc.i_vbv_buffer_size = bufferSizeInFrames * bitrate / fps;
		
		//Reconfig
		if (x264_encoder_reconfig(enc,&params)!=0)
			//reconfigure
			return Error("-Could not reconfigure h264 codec\n");
	}
	
	return 1;
}

/**********************
* OpenCodec
*	Abre el codec
***********************/
int H264Encoder::OpenCodec()
{
	Log("-H264Encoder::OpenCodec() [%dkbps,%dfps,%dintra,width:%d,height:%d, profile: %s, level: %s, preset: %s, tune: %s, streaming: %d, ipratio: %f, ratetol: %f]\n",bitrate,fps,intraPeriod,width,height, GetProfileInUse().c_str(), GetLevelInUse().c_str(), GetPresetInUse().c_str(), GetTuneInUse().c_str(), streaming, ipratio, ratetol);

	// Check 
	if (opened)
		return Error("Codec already opened\n");

	// Reset default values
	x264_param_default(&params);

	// Use a defulat preset
	x264_param_default_preset(&params,GetPresetInUse().c_str(),GetTuneInUse().c_str());

	// Set log
	params.pf_log               = X264_log;

	if (Logger::IsDebugEnabled())
		//Set debug level loging
		params.i_log_level          = X264_LOG_INFO;
	else
		//Set error level for loging
		params.i_log_level          = X264_LOG_ERROR;
	
	// Set encoding context size
	params.i_width 	= width;
	params.i_height	= height;

	// Set parameters
	params.i_keyint_max         = intraPeriod;
	params.i_keyint_min	    = intraPeriod;
	params.i_frame_reference    = 2;
	params.rc.i_rc_method	    = X264_RC_ABR;
	//param->rc.i_rc_method	    = X264_RC_CRF;
	//param->rc.f_rf_constant   = 23;

	params.rc.i_bitrate         = bitrate;
	params.rc.i_vbv_max_bitrate = bitrate * maxBitrateMultiplier;
	if (qMin) 
		params.rc.i_qp_min	    = qMin;
	if (qMax)
		params.rc.i_qp_max	    = qMax;
	params.rc.i_aq_mode	    = X264_AQ_AUTOVARIANCE_BIASED;
	if (!streaming)
	{
		params.rc.b_stat_write      = 0;
		params.b_intra_refresh	    = 1;
	}
	params.b_deblocking_filter = 1;
	params.i_deblocking_filter_alphac0 = -2;
	params.i_deblocking_filter_beta = 2;

	params.rc.i_vbv_buffer_size = bufferSizeInFrames * bitrate / fps;
	params.rc.f_rate_tolerance  = ratetol;


	Debug("-H264Encoder::OpenCodec() | config params.rc.f_rate_tolerance:%f params.rc.i_vbv_buffer_size:%d\n", params.rc.f_rate_tolerance, params.rc.i_vbv_buffer_size);


	// change ipratio only when it's configured in profile property
	// or else leave its value according to preset & tune
	if (ipratio >= 0)
	{
		params.rc.f_ip_factor = ipratio;
	}
	
	params.i_threads		= threads; //0 is auto!!
	params.b_sliced_threads		= 1;
	params.b_vfr_input		= 0;
	params.rc.i_lookahead		= 0;
	params.rc.b_mb_tree		= 0;
	params.i_sync_lookahead		= 0;
	params.i_scenecut_threshold	= 0;
	params.analyse.i_subpel_refine	= 6; //Subpixel motion estimation and mode decision :3 qpel (medim:6, ultrafast:1)
	params.analyse.i_weighted_pred	= X264_WEIGHTP_NONE;
	params.analyse.i_me_method	= X264_ME_UMH;
	params.i_bframe			= 0;
	params.b_annexb			= annexb; 
	params.b_repeat_headers		= 1;
	params.i_fps_num		= fps;
	params.i_fps_den		= 1;
	params.vui.i_chroma_loc		= 0;

	//Set level
	params.i_level_idc = LevelNumberToLevelIdc(GetLevelInUse().c_str());

	x264_param_apply_profile(&params, GetProfileInUse().c_str());

	// Open encoder
	enc = x264_encoder_open(&params);

	//Check it is correct
	if (!enc)
		return Error("Could not open h264 codec\n");

	// Clean pictures
	memset(&pic,0,sizeof(x264_picture_t));
	memset(&pic_out,0,sizeof(x264_picture_t));

	//Set picture type
	pic.i_type = X264_TYPE_AUTO;
	
	// We are opened
	opened = true;

	// Exit
	return 1;
}


/**********************
* EncodeFrame
*	Codifica un frame
***********************/
VideoFrame* H264Encoder::EncodeFrame(const VideoBuffer::const_shared& videoBuffer)
{
	if(!opened)
	{
		Error("-Codec not opened\n");
		return NULL;
	}

	int len = 0;
	if (videoBuffer)
	{
		//Get planes
		const Plane& y = videoBuffer->GetPlaneY();
		const Plane& u = videoBuffer->GetPlaneU();
		const Plane& v = videoBuffer->GetPlaneV();

		//POnemos los valores
		pic.img.plane[0] = (unsigned char*)y.GetData();
		pic.img.plane[1] = (unsigned char*)u.GetData();
		pic.img.plane[2] = (unsigned char*)v.GetData();
		pic.img.i_stride[0] = y.GetStride();
		pic.img.i_stride[1] = u.GetStride();
		pic.img.i_stride[2] = v.GetStride();
		pic.img.i_csp   = X264_CSP_I420;
		pic.img.i_plane = 3;
		pic.i_pts  = pts++;

		// Encode frame and get length
		len = x264_encoder_encode(enc, &nals, &numNals, &pic, &pic_out);
	}
	// end of stream, flushing buffered encoded frame
	else
	{
		len = x264_encoder_encode(enc, &nals, &numNals, nullptr, &pic_out);
	}
	

	//Check it
	if (len<0)
	{
		//Error
		Error("-H264Encoder::EncodeFrame() | Error encoding frame [len:%d]\n",len);
		return NULL;
	}
	else if (len == 0)
	{
		Warning("-H264Encoder::EncodeFrame() | Encode H264 returns no output frame, got delay");
		return nullptr;
	}

	//Set the media
	frame.SetMedia(nals[0].p_payload,len);

	//Set width and height
	frame.SetWidth(width);
	frame.SetHeight(height);

	//Set intra
	frame.SetIntra(pic_out.b_keyframe);

	//Unset type
	pic.i_type = X264_TYPE_AUTO;

	//Emtpy rtp info
	frame.ClearRTPPacketizationInfo();
	
	//If intra
	if (pic_out.b_keyframe)
	{
		//Clear config
		config.ClearSequenceParameterSets();
		config.ClearPictureParameterSets();
	}

	//Add packetization
	for (int i=0;i<numNals;i++)
	{
		// Get NAL data pointer skiping the header (nalUnit = type + nal)
		BYTE* nalUnit = nals[i].p_payload+4;
		//Get size
		DWORD nalUnitSize = nals[i].i_payload-4;
		//Get nal pos
		DWORD pos = nalUnit - nals[0].p_payload;
		//Get nal type
		BYTE nalType = (nalUnit[0] & 0x1f);
		//Skip header
		BYTE* nalData = nalUnit+1;
		//Check if IDR SPS or PPS
		switch (nalType)
		{
			case 0x07:
			{
				//Set config
				config.SetConfigurationVersion(1);
				config.SetAVCProfileIndication(nalData[0]);
				config.SetProfileCompatibility(nalData[1]);
				config.SetAVCLevelIndication(nalData[2]);
				config.SetNALUnitLengthSizeMinus1(3);
				//Add full nal to config
				config.AddSequenceParameterSet(nalUnit,nalUnitSize);
				break;
			}
			case 0x08:
				//Add full nal to config
				config.AddPictureParameterSet(nalUnit,nalUnitSize);
				break;
		}

		//Add rtp packet
		if (nalUnitSize < RTPPAYLOADSIZE)
		{
			frame.AddRtpPacket(pos,nalUnitSize,NULL,0);
		}
		else
		{
			//Otherwise use FU
			/*
			* the FU indicator + FU header octet has the following format:
			*
			*    +---------------+      +---------------+
			*    |0|1|2|3|4|5|6|7|      |0|1|2|3|4|5|6|7|
			*    +-+-+-+-+-+-+-+-+      +-+-+-+-+-+-+-+-+
			*    |F|NRI| FU-A(28)|      |S|E|R|  Type   |
			*    +---------------+      +---------------+
			* For H.264, R must be 0.
			*/

			//Start with S = 1, E = 0
			const size_t naluHeaderSize = 1;
			const uint8_t nri = nalUnit[0] & 0b0110'0000;
			std::array<uint8_t, 2> fuPrefix = { nri | 28u, nalType };
			fuPrefix[1] &= 0b0011'1111;
			fuPrefix[1] |= 0b1000'0000;
			// Skip payload nal header
			size_t left = nalUnitSize - naluHeaderSize;
			pos += naluHeaderSize;
			//Split it
			while (left > 0)
			{
				size_t pkt_len = std::min<size_t>(RTPPAYLOADSIZE - fuPrefix.size(), left);
				left -= pkt_len;
				//If all added
				if (left <= 0)
					//Set end mark
					fuPrefix[1] |= 0b01'000000;
				//Add packetization
				frame.AddRtpPacket(pos, pkt_len, fuPrefix.data(), fuPrefix.size());
				//Not first
				fuPrefix[1] &= 0b01'111111;
				//Move start
				pos += pkt_len;
			}
		}
	}

	//Set first nal
	curNal = 0;
	
	//If intra
	if (pic_out.b_keyframe)
	{
		//Set config size
		frame.AllocateCodecConfig(config.GetSize());
		//Serialize
		config.Serialize(frame.GetCodecConfigData(),frame.GetCodecConfigSize());
	}
	
	return &frame;
}

/**********************
* FastPictureUpdate
*	Manda un frame entero
***********************/
int H264Encoder::FastPictureUpdate()
{
	//Set next picture type
	pic.i_type = X264_TYPE_I;

	return 1;
}

