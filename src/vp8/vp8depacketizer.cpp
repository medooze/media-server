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
}

MediaFrame* VP8Depacketizer::AddPacket(const RTPPacket::shared& packet)
{
	//Get timestamp in ms
	auto ts = packet->GetTimestamp();
	//Check it is from same packet
	if (frame.GetTimeStamp()!=ts)
		//Reset frame
		ResetFrame();
	//Set timestamp
	frame.SetTimestamp(ts);
	//Set SSRC
	frame.SetSSRC(packet->GetSSRC());
	//Add payload
	AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	//Check if it has vp8 descriptor
	if (packet->vp8PayloadHeader)
	{
		//Set data
		frame.SetWidth(packet->vp8PayloadHeader->width);
		frame.SetHeight(packet->vp8PayloadHeader->height);
	}
	//If it is last return frame
	return packet->GetMark() ? &frame : NULL;
}

MediaFrame* VP8Depacketizer::AddPayload(const BYTE* payload, DWORD len)
{
	//Check lenght
	if (!len)
		//Exit
		return NULL;
    
	//Is first?
	int first = !frame.GetLength(); 
    
	VP8PayloadDescriptor desc;

	//Decode payload descriptr
	DWORD descLen = desc.Parse(payload,len);
	
	//Check
	if (!descLen || len<descLen)
	{
		Dump(payload,len);
		//Error
		Error("- VP8Depacketizer::AddPayload() | Error decoding VP8 payload header\n");
		return NULL;
	}
	
	//Skip desc
	DWORD pos = frame.AppendMedia(payload+descLen, len-descLen);
	
	//Add RTP packet
	frame.AddRtpPacket(pos,len-descLen,payload,descLen);
	
	//If it is first
	if (first)
	{
		//calculate if it is an iframe
		frame.SetIntra(!(payload[descLen] & 0x01));
		//TODO: armonize with rtp packet vp8 data
		frame.SetWidth(640);
		frame.SetHeight(480);
	}
    

		
	return &frame;
}

