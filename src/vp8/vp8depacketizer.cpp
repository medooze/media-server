/*
 * File:   h264depacketizer.cpp
 * Author: Sergio
 *
 * Created on 26 de enero de 2012, 9:46
 */

#include "vp8depacketizer.h"
#include "vp8.h"
#include "media.h"
#include "codecs.h"
#include "rtp.h"
#include "log.h"

#ifndef AV_RL24
#define AV_RL24(x) ((((const uint8_t*)(x))[2] << 16) | (((const uint8_t*)(x))[1] <<  8) | ((const uint8_t*)(x))[0])
#endif

namespace {

static bool CheckFrameHeader(uint8_t* buf, size_t buf_size)
{
	if (buf_size < 3) {
		return false;
	}

	auto keyframe  = !(buf[0] & 1);
	auto profile   =  (buf[0]>>1) & 7;
	auto invisible = !(buf[0] & 0x10);
	auto header_size  = AV_RL24(buf) >> 5;
	buf      += 3;
	buf_size -= 3;

	if (profile > 3)
	{
		Warning("Unknown profile %d\n", profile);
	}

	if (header_size > buf_size - 7 * keyframe) {
		Error("Header size larger than data provided\n");
		return false;
	}

	return true;
}
}


VP8Depacketizer::VP8Depacketizer() : RTPDepacketizer(MediaFrame::Video,VideoCodec::VP8), frame(VideoCodec::VP8,0)
{
	//Generic vp8 config
	VP8CodecConfig config;
	//Set config size
	frame.AllocateCodecConfig(config.GetSize());
	//Serialize
	config.Serialize(frame.GetCodecConfigData(),frame.GetCodecConfigSize());
	//Set clock rate
	frame.SetClockRate(90000);
}

VP8Depacketizer::~VP8Depacketizer()
{

}

void VP8Depacketizer::ResetFrame()
{
	//Reset frame data
	frame.Reset();
	//Generic vp8 config
	VP8CodecConfig config;
	//Set config size
	frame.AllocateCodecConfig(config.GetSize());
	//Serialize
	config.Serialize(frame.GetCodecConfigData(),frame.GetCodecConfigSize());
}

MediaFrame* VP8Depacketizer::AddPacket(const RTPPacket::shared& packet)
{
	//Get timestamp in ms
	auto ts = packet->GetExtTimestamp();
	//Check it is from same packet
	if (frame.GetTimeStamp()!=ts)
		//Reset frame
		ResetFrame();
	//If not timestamp
	if (frame.GetTimeStamp()==(DWORD)-1)
	{
		//Set timestamp
		frame.SetTimestamp(ts);
		//Set clock rate
		frame.SetClockRate(packet->GetClockRate());
		//Set time
		frame.SetTime(packet->GetTime());
		//Set sender time
		frame.SetSenderTime(packet->GetSenderTime());
	}
	//Set SSRC
	frame.SetSSRC(packet->GetSSRC());

	//Add payload
	AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	//Check if it has vp8 descriptor
	if (packet->vp8PayloadHeader)
	{
		//Set key frame
		frame.SetIntra(packet->vp8PayloadHeader->isKeyFrame);
		//if it is intra
		if (frame.IsIntra())
		{
			//Set dimensions
			frame.SetWidth(packet->vp8PayloadHeader->width);
			frame.SetHeight(packet->vp8PayloadHeader->height);
		}

	}

	//If it is last return frame
	if (packet->GetMark())
	{
		// Check for corrupted data
		if (!CheckFrameHeader(frame.GetData(),frame.GetLength()))
		{
			Error("-VP8Depacketizer::AddPacket() | VP8 frame header decode error\n");
			return nullptr;
		}

		return &frame;
	}

	return nullptr;
}

MediaFrame* VP8Depacketizer::AddPayload(const BYTE* payload, DWORD len)
{
	//Check lenght
	if (!len)
		//Exit
		return NULL;

	VP8PayloadDescriptor desc;

	//Decode payload descriptr
	DWORD descLen = desc.Parse(payload,len);

	//Check
	if (!descLen || len<descLen)
	{
		Dump(payload,len);
		//Error
		Error("-VP8Depacketizer::AddPayload() | Error decoding VP8 payload header\n");
		return NULL;
	}

	//Skip desc
	DWORD pos = frame.AppendMedia(payload+descLen, len-descLen);

	//Add RTP packet
	frame.AddRtpPacket(pos,len-descLen,payload,descLen);

	return &frame;
}
