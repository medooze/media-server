/* 
 * File:   h264depacketizer.cpp
 * Author: Sergio
 * 
 * Created on 26 de enero de 2012, 9:46
 */

#include "vp8depacketizer.h"
#include "media.h"
#include "codecs.h"
#include "rtp.h"
#include "log.h"


VP8Depacketizer::VP8Depacketizer() : RTPDepacketizer(MediaFrame::Video,VideoCodec::VP8), frame(VideoCodec::VP8,0)
{
}

VP8Depacketizer::~VP8Depacketizer()
{

}
void VP8Depacketizer::SetTimestamp(DWORD timestamp)
{
	//Set timestamp
	frame.SetTimestamp(timestamp);
}
void VP8Depacketizer::ResetFrame()
{
	//Clear packetization info
	frame.ClearRTPPacketizationInfo();
	//Reset
	memset(frame.GetData(),0,frame.GetMaxMediaLength());
	//Clear length
	frame.SetLength(0);
}

MediaFrame* VP8Depacketizer::AddPacket(RTPPacket *packet)
{
	//Set timestamp
	frame.SetTimestamp(packet->GetTimestamp());
	//Add payload
	return AddPayload(packet->GetMediaData(),packet->GetMediaLength());
}

MediaFrame* VP8Depacketizer::AddPayload(BYTE* payload, DWORD payload_len)
{
	//Check lenght
	if (!payload_len)
		//Exit
		return NULL;
    
    //Is first?
    int first = frame.GetMaxMediaLength(); 
    
    //Skip desc
	DWORD pos = frame.AppendMedia(payload+6, payload_len-6);
	//Add RTP packet
	frame.AddRtpPacket(pos,payload_len-6,payload,6);
    
    //If it is first
    if (first)
        //calculate if it is an iframe
        frame.SetIntra(payload[6] & 0x01);
    
	return &frame;
}

