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

/**********************
* H264Encoder
*	Constructor de la clase
***********************/
H264Encoder::H264Encoder(const Properties& properties) : frame(VideoCodec::H264)
{
	// Set default values
	type    = VideoCodec::H264;
	format  = 0;
	pts	= 0;

	//No estamos abiertos
	opened = false;

	//No bitrate
	bitrate = 0;
	fps = 0;
	intraPeriod = X264_KEYINT_MAX_INFINITE;
	
	//Number of threads or auto
	threads = properties.GetProperty("h264.threads",0);

	//Check profile level id
	h264ProfileLevelId = properties.GetProperty("h264.profile-level-id",std::string("42801F"));
	
	//Use annex b
	annexb = properties.GetProperty("h264.annexb",false);

	//Check mode
	streaming = properties.GetProperty("h264.streaming",true);
	//streaming = properties.GetProperty("h264.streaming",false);
	//experiment = properties.GetProperty("h264.experiment",false);
	experiment = properties.GetProperty("h264.experiment",true);

	//Disable sharing buffer on clone
	frame.DisableSharedBuffer();

	//Reste values
	enc = NULL;
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
	//Log("-H264Encoder::SetFrameRate() [fps: %d, kbits: %d, intra: %d]\n",frames,kbits,intraPeriod);
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
		//params.i_keyint_max         = this->intraPeriod ;
		//params.i_frame_reference    = 1;
		//params.rc.i_rc_method	    = X264_RC_ABR;
		params.rc.i_bitrate         = bitrate;
		params.rc.i_vbv_max_bitrate = bitrate;
		//params.rc.i_vbv_buffer_size = bitrate/fps;
		params.rc.i_vbv_buffer_size = bitrate/10;
		//params.rc.f_vbv_buffer_init = 0;
		//params.rc.f_rate_tolerance  = 0.1;
		
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
	Log("-H264Encoder::OpenCodec() ttxgz: 1142");
	Log("-H264Encoder::OpenCodec() [%dkbps,%dfps,%dintra,width:%d,height:%d, profile-level-id: %s, streaming: %d, experiment: %d]\n",bitrate,fps,intraPeriod,width,height, h264ProfileLevelId.c_str(), streaming, experiment);

	// Check 
	if (opened)
		return Error("Codec already opened\n");

	// Reset default values
	x264_param_default(&params);

	// Use a defulat preset
	x264_param_default_preset(&params,"medium","zerolatency");

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
	//params.i_frame_reference    = 1;
	params.rc.i_rc_method	    = X264_RC_ABR;
	params.rc.i_bitrate         = bitrate;
	params.rc.i_vbv_max_bitrate = bitrate;
	if (!streaming)
	{
		params.rc.i_vbv_buffer_size = bitrate/fps;
		params.rc.f_rate_tolerance  = 0.1;
		params.rc.b_stat_write      = 0;
		params.i_slice_max_size     = RTPPAYLOADSIZE-8;
		params.b_intra_refresh	    = 1;
	} else  {
#if 0
		params.rc.i_vbv_buffer_size = bitrate;
#else
		//params.rc.i_vbv_buffer_size = bitrate/fps;
		params.rc.i_vbv_buffer_size = bitrate/10;
		//params.rc.f_rate_tolerance  = 0.1;
		params.rc.f_rate_tolerance  = 0;
		//params.rc.f_rate_tolerance  = fps/3;
		//params.rc.b_stat_write      = 0;
		//params.i_slice_max_size     = RTPPAYLOADSIZE-8;
		//params.b_intra_refresh	    = 1;
#endif
		//if (!experiment)
		//	params.i_slice_max_size     = RTPPAYLOADSIZE-8;
	}
	//params.rc.f_vbv_buffer_init = 0;
	params.i_threads	    = threads; //0 is auto!!
	//params.b_sliced_threads	    = 0;
	//params.rc.i_lookahead       = 0;
	//params.i_sync_lookahead	    = 0;

	//ttxgz added lingwai jia
	params.rc.b_mb_tree       = 0;

	params.i_bframe             = 0;
	params.b_annexb		    = annexb; 
	params.b_repeat_headers     = 1;
	params.i_fps_num	    = fps;
	params.i_fps_den	    = 1;
	params.vui.i_chroma_loc	    = 0;
	//params.i_scenecut_threshold = 0;
	//params.analyse.i_subpel_refine = 5; //Subpixel motion estimation and mode decision :3 qpel (medim:6, ultrafast:1)

	//Get profile and level
	int profile = strtol(h264ProfileLevelId.substr(0,2).c_str(),NULL,16);
	int level   = strtol(h264ProfileLevelId.substr(4,2).c_str(),NULL,16);

	//Set level
	params.i_level_idc = level;

	//Depending on the profile in hex
	switch (profile)
	{
		case 100:
		case 88:
			//High
			x264_param_apply_profile(&params,"high");
			break;
		case 77:
			//Main
			x264_param_apply_profile(&params,"main");
			break;
		case 66:
		default:
			//Baseline
			x264_param_apply_profile(&params,"baseline");
	}

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
	int len = x264_encoder_encode(enc, &nals, &numNals, &pic, &pic_out);

	//Check it
	if (len<0)
	{
		//Error
		Error("Error encoding frame [len:%d]\n",len);
		return NULL;
	}
	else if (len == 0)
	{
		Warning("Encode H264 returns no output frame");
		return nullptr;
	}

#if 0
	if (encoded_frame > 1)
	{
		return nullptr;
	}
	else
	{
		encoded_frame++;
	}
#endif
	
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

	encoded_frame++;
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
		DWORD nalSize = nalUnitSize-1;
		//Check if IDR SPS or PPS
		switch (nalType)
		{
			case 0x07:
			{
				//Get profile as hex representation
				DWORD profileLevel = strtol (h264ProfileLevelId.c_str(),NULL,16);
				//Modify profile level ID to match offered one
				Log("ttxgz: before overwrite: nalData: 0x%02x%02x%02x\n", *nalData, *(nalData+1), *(nalData+2));
				//set3(nalData,0,profileLevel);
				//Log("ttxgz: after overwrite: nalData: 0x%02x%02x%02x\n", *nalData, *(nalData+1), *(nalData+2));
				//Set config
				config.SetConfigurationVersion(1);
				config.SetAVCProfileIndication(nalData[0]);
				config.SetProfileCompatibility(nalData[1]);
				config.SetAVCLevelIndication(nalData[2]);
				config.SetNALUnitLengthSizeMinus1(3);
				//Add full nal to config
				config.AddSequenceParameterSet(nalUnit,nalUnitSize);
				//config.AddSequenceParameterSet(nalData,nalSize);
				break;
			}
			case 0x08:
				//Add full nal to config
				config.AddPictureParameterSet(nalUnit,nalUnitSize);
				//config.AddPictureParameterSet(nalData,nalSize);
				break;
		}

		//Log("ttxgz: encoded_frame: %d, naltype: %d, nalUnitSize: %d, pos: %d, experiment: %d\n", encoded_frame, nalType, nalUnitSize, pos, experiment);
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

#if 0
	uint8_t naluHeader = nal.Peek1();
	uint8_t nri = naluHeader & 0b0'11'00000;
	uint8_t nalUnitType = naluHeader & 0b0'00'11111;

	std::string fuPrefix = { (char)(nri | 28u), (char)nalUnitType };
	H26xPacketizer::EmitNal(frame, nal, fuPrefix, 1);
#endif
			//Log("ttxgz: split to FU, experiment:%d\n", experiment);

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
				//Log("ttxgz: FU packet: pos:%d, pkt_len:%d\n",pos,pkt_len);
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

