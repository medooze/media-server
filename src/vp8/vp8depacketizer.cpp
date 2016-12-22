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
	//Check it is from same packet
	if (frame.GetTimeStamp()!=packet->GetTimestamp())
		//Reset frame
		ResetFrame();
	//Set timestamp
	frame.SetTimestamp(packet->GetTimestamp());
	//Add payload
	AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	//If it is last return frame
	return packet->GetMark() ? &frame : NULL;
}

MediaFrame* VP8Depacketizer::AddPayload(BYTE* payload, DWORD len)
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

	
	//Skip desc
	DWORD pos = frame.AppendMedia(payload+descLen, len-descLen);
	
	//Add RTP packet
	frame.AddRtpPacket(pos,len-descLen,payload,descLen);
	
	//If it is first
	if (first)
	{
		//calculate if it is an iframe
		frame.SetIntra(!(payload[descLen] & 0x01));
		//Fakse size
		frame.SetWidth(640);
		frame.SetHeight(480);
	}
    

		
	return &frame;
}

