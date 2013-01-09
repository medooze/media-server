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
H264Encoder::H264Encoder()
{
	// Set default values
	type    = VideoCodec::H264;
	format  = 0;
	frame	= NULL;
	pts	= 0;

	//No estamos abiertos
	opened = false;

	//No bitrate
	bitrate = 0;
	fps = 0;
	intraPeriod = 0;
	
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
	//If we have created a frame
	if (frame)
		//Delete it
		delete(frame);
}

/**********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
***********************/
int H264Encoder::SetSize(int width, int height)
{
	Log("-SetSize [%d,%d]\n",width,height);

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
int H264Encoder::SetFrameRate(int frames,int kbits,int intraPeriod)
{
	Log("-SetFrameRate [%dfps,%dkbps,intra:%d]\n",frames,kbits,intraPeriod);
	
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
		params.i_keyint_max         = intraPeriod;
		params.i_frame_reference    = 1;
		params.rc.i_rc_method	    = X264_RC_ABR;
		params.rc.i_bitrate         = bitrate;
		params.rc.i_vbv_max_bitrate = bitrate;
		params.rc.i_vbv_buffer_size = bitrate/fps;
		params.rc.f_vbv_buffer_init = 0;
		params.rc.f_rate_tolerance  = 0.1;
		params.i_fps_num	    = fps;
		//Reconfig
		x264_encoder_reconfig(enc,&params);
	}
	
	return 1;
}

/**********************
* OpenCodec
*	Abre el codec
***********************/
int H264Encoder::OpenCodec()
{
	Log("-OpenCodec H264 [%dkbps,%dfps,%dintra]\n",bitrate,fps,intraPeriod);

	// Check 
	if (opened)
		return Error("Codec already opened\n");

	// Reset default values
	x264_param_default(&params);

	// Use a defulat preset
	x264_param_default_preset(&params,"medium","zerolatency");

	// Set log
	params.pf_log               = X264_log;

#ifdef MCUDEBUG
	//Set debug level loging
	params.i_log_level          = X264_LOG_INFO;
#else
	//Set error level for loging
	params.i_log_level          = X264_LOG_ERROR;
#endif


	// Set encoding context size
	params.i_width 	= width;
	params.i_height	= height;

	// Set parameters
	params.i_keyint_max         = intraPeriod;
	params.i_frame_reference    = 1;
	params.rc.i_rc_method	    = X264_RC_ABR;
	params.rc.i_bitrate         = bitrate;
	params.rc.i_vbv_max_bitrate = bitrate;
	params.rc.i_vbv_buffer_size = bitrate/fps;
	params.rc.f_vbv_buffer_init = 0;
	params.rc.f_rate_tolerance  = 0.1;
	params.rc.b_stat_write      = 0;
	params.i_slice_max_size     = RTPPAYLOADSIZE-8;
	params.i_threads	    = 1; //0 is auto!!
	params.b_sliced_threads	    = 0;
	params.rc.i_lookahead       = 0;
	params.i_sync_lookahead	    = 0;
	params.i_bframe             = 0;
	params.b_annexb		    = 0; 
	params.b_repeat_headers     = 1;
	params.i_fps_num	    = fps;
	params.i_fps_den	    = 1;
	params.b_intra_refresh	    = 1;
	params.vui.i_chroma_loc	    = 0;
	params.i_scenecut_threshold = 0;
	params.analyse.i_subpel_refine = 6; //Subpixel motion estimation and mode decision :3 qpel (medim:6, ultrafast:1)
	// Set profile level constrains
	x264_param_apply_profile(&params,"baseline");

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
	opened=true;

	// Exit
	return 1;
}


/**********************
* EncodeFrame
*	Codifica un frame
***********************/
VideoFrame* H264Encoder::EncodeFrame(BYTE *buffer,DWORD bufferSize)
{
	if(!opened)
	{
		Error("-Codec not opened\n");
		return NULL;
	}

	//Comprobamos el tama�o
	if (numPixels*3/2 != bufferSize)
	{
		Error("-EncodeFrame length error [%d,%d]\n",numPixels*5/4,bufferSize);
		return NULL;
	}

	//POnemos los valores
	pic.img.plane[0] = buffer;
	pic.img.plane[1] = buffer+numPixels;
	pic.img.plane[2] = buffer+numPixels*5/4;
	pic.img.i_stride[0] = width;
	pic.img.i_stride[1] = width/2;
	pic.img.i_stride[2] = width/2; 
	pic.img.i_csp   = X264_CSP_I420;
	pic.img.i_plane = 3;
	pic.i_pts  = pts++;

	// Encode frame and get length
	int len = x264_encoder_encode(enc, &nals, &numNals, &pic, &pic_out);

	//Check it
	if (len<=0)
	{
		//Error
		Error("Error encoding frame [len:%d]\n",len);
		return NULL;
	}
	//Check size
	if (!frame)
		//Create new frame
		frame = new VideoFrame(type,len);

	//Set the media
	frame->SetMedia(nals[0].p_payload,len);

	//Set width and height
	frame->SetWidth(width);
	frame->SetHeight(height);

	//Set intra
	frame->SetIntra(pic_out.b_keyframe);

	//Unset type
	pic.i_type = X264_TYPE_AUTO;

	//Emtpy rtp info
	frame->ClearRTPPacketizationInfo();

	//Add packetization
	for (DWORD i=0;i<numNals;i++)
	{
		
		// Get NAL data pointer skiping the header
		BYTE* nalData = nals[i].p_payload+4;
		//Get size
		DWORD nalSize = nals[i].i_payload-4;
		//Get nal pos
		DWORD pos = nalData - nals[0].p_payload;
		//Add rtp packet
		frame->AddRtpPacket(pos,nalSize,NULL,0);
	}

	//Set first nal
	curNal = 0;
	
	return frame;
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

/**********************
* GetNextPacket
*	Obtiene el siguiente paquete para enviar
***********************/
int H264Encoder::GetNextPacket(BYTE *out,DWORD &len)
{
	// Get NAL data pointer skiping the header
	BYTE* nalData = nals[curNal].p_payload+4;

	// Get NAL size 
	DWORD nalSize = nals[curNal].i_payload-4;

	// Check if it fits in a udp packet 
	if (nalSize<len)
	{
		// Set frame len 
		len = nalSize;
		// Copy NAL 
		memcpy(out, nalData, nalSize);
	} else 
		Log("-Error NAL too big [%lu]\n",nalSize);

	// Increase NAL 
	curNal++;
	// Check if it is the last packet 
	if (curNal==numNals)
		// Last packet 
		return 0;
	else
		// Not Last packet
		return 1;
}

