#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "log.h"
#include "OpenH264Encoder.h"

using namespace openh264;

/**********************
* OpenH264Encoder
*	Constructor de la clase
***********************/
OpenH264Encoder::OpenH264Encoder(const Properties& properties) : frame(VideoCodec::H264)
{
	// Set default values
	type = VideoCodec::H264;
	Debug("-OpenH264Encoder::OpenH264Encoder()\n");
	for (Properties::const_iterator it = properties.begin(); it != properties.end(); ++it)
		Debug("-OpenH264Encoder::OpenH264Encoder() | Setting property [%s:%s]\n", it->first.c_str(), it->second.c_str());

	//Number of threads or auto
	threads = properties.GetProperty("h264.threads", 0);

	//ipratio
	ipratio = properties.GetProperty("h264.ipratio", -1.0F);

	//VBV buffer size in frames according to the bitrate
	maxBitrateMultiplier = properties.GetProperty("h264.max_bitrate_multiplier", 1.0F);

	qMin = properties.GetProperty("h264.qmin", qMin);
	qMax = properties.GetProperty("h264.qmax", qMax);

	//Disable sharing buffer on clone
	frame.DisableSharedBuffer();
}

/**********************
* ~OpenH264Encoder
*	Destructor
***********************/
OpenH264Encoder::~OpenH264Encoder()
{

	//If we have an encoder
	if (encoder) 
	{
		encoder->Uninitialize();
		WelsDestroySVCEncoder(encoder);
	}
}

/**********************
* SetSize
*	Inicializa el tama�o de la imagen a codificar
***********************/
int OpenH264Encoder::SetSize(int width, int height)
{
	//Check if they are the same size
	if (this->width == width && this->height == height)
		//Do nothing
		return 1;

	Log("-OpenH264Encoder::SetSize() [width:%d,height:%d]\n", width, height);

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
int OpenH264Encoder::SetFrameRate(int frames, int kbits, int intraPeriod)
{
	//UltraDebug("-OpenH264Encoder::SetFrameRate() [fps: %d, kbits: %d, intra: %d]\n",frames,kbits,intraPeriod);
	// Save frame rate
	if (frames > 0)
		fps = frames;
	else
		fps = 10;

	// Save bitrate
	if (kbits > 0)
		bitrate = kbits * 1000;

	//Save intra period
	this->intraPeriod = intraPeriod;

	//Check if already opened
	if (opened)
	{
		// Update h264 encoder.
		SBitrateInfo target = {};
		target.iBitrate = bitrate;
		encoder->SetOption(ENCODER_OPTION_BITRATE	, &target);
		encoder->SetOption(ENCODER_OPTION_FRAME_RATE	, &fps);
		encoder->SetOption(ENCODER_OPTION_IDR_INTERVAL  , &intraPeriod);
	}

	return 1;
}

/**********************
* OpenCodec
*	Abre el codec
***********************/
int OpenH264Encoder::OpenCodec()
{
	Log("-OpenH264Encoder::OpenCodec() [%dkbps,%dfps,%dintra,width:%d,height:%d,]\n", bitrate/1000, fps, intraPeriod, width, height);

	// Check 
	if (opened)
		return Error("Codec already opened\n");

	//Open encoder
	opened = WelsCreateSVCEncoder(&encoder) == 0;

	if (!opened || !encoder)
		return Error("Could not initialize h264 codec\n");

	
	//Configure encoder
	SEncParamExt  param = {};
	encoder->GetDefaultParams(&param);

	param.iUsageType	= CAMERA_VIDEO_REAL_TIME;
	param.iComplexityMode   = HIGH_COMPLEXITY;
	param.iRCMode		= RC_BITRATE_MODE;
	param.eSpsPpsIdStrategy = CONSTANT_ID;
	param.uiMaxNalSize	= 0;
	param.bFixRCOverShoot	= true;
	param.iEntropyCodingModeFlag	= 1; //CABAC
	param.bEnableLongTermReference	= true;
	param.iTemporalLayerNum		= 1;
	param.iSpatialLayerNum		= 1;

	param.iMultipleThreadIdc = threads;
	param.iPicWidth		= width;
	param.iPicHeight	= height;
	param.iTargetBitrate	= bitrate;
	param.iMaxBitrate	= RC_BITRATE_MODE;//bitrate * maxBitrateMultiplier;
	param.fMaxFrameRate	= fps;
	param.uiIntraPeriod	= intraPeriod;
	param.bEnableFrameSkip  = false;
	if (qMin)
		param.iMinQp	= qMin;
	if (qMax)
		param.iMaxQp = qMax;
	//param.iIdrBitrateRatio = ipratio * 100;

	if (encoder->InitializeExt(&param)!=0)
		return Error("Could not initialize h264 codec\n");
	
	int videoFormat = videoFormatI420;
	encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);
	int level = WELS_LOG_DETAIL;
	encoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, &level);
	// We are opened
	opened = true;

	// Exit
	return 1;
}


/**********************
* EncodeFrame
*	Codifica un frame
***********************/
VideoFrame* OpenH264Encoder::EncodeFrame(const VideoBuffer::const_shared& videoBuffer)
{
	if (!opened)
	{
		Error("-Codec not opened\n");
		return NULL;
	}

	if (!videoBuffer)
		return NULL;
	
	SFrameBSInfo info = {};
	SSourcePicture pic = {};

	//Get planes
	const Plane& y = videoBuffer->GetPlaneY();
	const Plane& u = videoBuffer->GetPlaneU();
	const Plane& v = videoBuffer->GetPlaneV();

	//POnemos los valores
	pic.iPicWidth = width;
	pic.iPicHeight = height;
	pic.iColorFormat = EVideoFormatType::videoFormatI420;
	pic.pData[0] = (unsigned char*)y.GetData();
	pic.pData[1] = (unsigned char*)u.GetData();
	pic.pData[2] = (unsigned char*)v.GetData();
	pic.iStride[0]	= y.GetStride();
	pic.iStride[1]	= u.GetStride();
	pic.iStride[2]	= v.GetStride();
	pic.uiTimeStamp = pts++;

	// Encode frame and get length
	if (encoder->EncodeFrame(&pic, &info) != 0)
	{
		//Error
		Error("-OpenH264Encoder::EncodeFrame() | Error encoding frame \n");
		return NULL;
	}

	if (info.eFrameType == videoFrameTypeSkip) 
	{
		//Error
		Error("-OpenH264Encoder::EncodeFrame() | frame skipping \n");
		return NULL;
	}
	//no svc 
	frame.SetLength(0);
	
	//Set width and height
	frame.SetWidth(width);
	frame.SetHeight(height);

	//Set intra
	frame.SetIntra(info.eFrameType == videoFrameTypeIDR);

	if (frame.IsIntra())
	{
		//Clear config
		config.ClearSequenceParameterSets();
		config.ClearPictureParameterSets();
	}

	//Emtpy rtp info
	frame.ClearRTPPacketizationInfo();


	//Add packetization
	for (int j = 0; j< info.iLayerNum; ++j)
	{
		//Get next layer info
		const SLayerBSInfo& layerInfo = info.sLayerInfo[j];

		//Layer length
		DWORD layerLength = 0;

		//For each nal
		for (int i = 0; i < layerInfo.iNalCount; i++)
		{
			//No annex b
			set4(layerInfo.pBsBuf, layerLength, layerInfo.pNalLengthInByte[i] - 4);
			// Get NAL data pointer skiping the header (nalUnit = type + nal)
			BYTE* nalUnit = layerInfo.pBsBuf + layerLength + 4;
			//Get nal unit size
			DWORD nalUnitSize = layerInfo.pNalLengthInByte[i] - 4;

			//Get nal start position in frame
			DWORD pos = frame.GetLength() + layerLength + 4;
			//Get nal type
			BYTE nalType = (nalUnit[0] & 0x1f);

			//Skip header
			BYTE* nalData = nalUnit + 1;

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
					config.AddSequenceParameterSet(nalUnit, nalUnitSize);
					break;
				}
				case 0x08:
					//Add full nal to config
					config.AddPictureParameterSet(nalUnit, nalUnitSize);
					break;
			}

			//Add rtp packet
			if (nalUnitSize < RTPPAYLOADSIZE)
			{
				frame.AddRtpPacket(pos, nalUnitSize, NULL, 0);
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
			//Increase layer lenth
			layerLength += layerInfo.pNalLengthInByte[i];
		}
		//Append the layer 
		int pos = frame.AppendMedia(layerInfo.pBsBuf, layerLength);
	}

	//If intra
	if (frame.IsIntra())
	{
		//Set config size
		frame.AllocateCodecConfig(config.GetSize());
		//Serialize
		config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
		config.Dump();
	}
	return &frame;
}

/**********************
* FastPictureUpdate
*	Manda un frame entero
***********************/
int OpenH264Encoder::FastPictureUpdate()
{
	return encoder->ForceIntraFrame(true) == 0;
}

