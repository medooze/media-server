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
	{
		//Reset frame
		ResetFrame();
		state = State::None;
	}
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

	// We expect the sequence number increases by one within a frame
	if (state == State::Processing && (packet->GetExtSeqNum() != (lastSeqNumber + 1)))
	{
		Warning("-VP8Depacketizer::AddPacket() | Sequence number is not increasing by 1: %d -> %d\n", lastSeqNumber, packet->GetExtSeqNum());
		state = State::Error;
	}
	lastSeqNumber = packet->GetExtSeqNum();

	if (state != State::Error)
	{
		//Add payload
		AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	}

	//Check if it has vp8 descriptor
	if (state != State::Error && packet->vp8PayloadHeader)
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

	MediaFrame* validFrame = nullptr;

	//If it is last return frame
	if (packet->GetMark())
	{
		switch (state)
		{
			case State::Processing:
				// No error, return the frame
				validFrame = &frame;
				break;
			case State::None:
				Warning("-VP8Depacketizer::AddPacket() | Dropped invalid frame as start packet missed. timestamp: %d\n", packet->GetTimestamp());
				break;
			case State::Error:
				Warning("-VP8Depacketizer::AddPacket() | Dropped invalid frame as error occurred. timestamp: %d\n", packet->GetTimestamp());
				break;
		}

		// Reset the state
		state = State::None;
	}

	return validFrame;
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

	if (state == State::None && desc.partitionIndex == 0 && desc.startOfPartition)
	{
		state = State::Processing;
	}

	//Skip desc
	DWORD pos = frame.AppendMedia(payload+descLen, len-descLen);

	//Add RTP packet
	frame.AddRtpPacket(pos,len-descLen,payload,descLen);

	return &frame;
}
