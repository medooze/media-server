/* 
 * File:   h264depacketizer.cpp
 * Author: Sergio
 * 
 * Created on 26 de enero de 2012, 9:46
 */

#include "VP9Depacketizer.h"
#include "VP9PayloadDescription.h"
#include "media.h"
#include "codecs.h"
#include "rtp.h"
#include "log.h"


VP9Depacketizer::VP9Depacketizer() : RTPDepacketizer(MediaFrame::Video,VideoCodec::VP9), frame(VideoCodec::VP9,0)
{
	//Fakse initial size
	frame.SetWidth(640);
	frame.SetHeight(480);
}

VP9Depacketizer::~VP9Depacketizer()
{

}
void VP9Depacketizer::SetTimestamp(DWORD timestamp)
{
	//Set timestamp
	frame.SetTimestamp(timestamp);
}
void VP9Depacketizer::ResetFrame()
{
	//Clear packetization info
	frame.ClearRTPPacketizationInfo();
	//Reset
	memset(frame.GetData(),0,frame.GetMaxMediaLength());
	//Clear length
	frame.SetLength(0);
	//Clear time
	frame.SetTimestamp((DWORD)-1);
	frame.SetTime((QWORD)-1);
}

MediaFrame* VP9Depacketizer::AddPacket(const RTPPacket::shared& packet)
{
	//Check it is from same packet
	if (frame.GetTimeStamp()!=packet->GetTimestamp())
		//Reset frame
		ResetFrame();
	//If not timestamp
	if (frame.GetTimeStamp()==(DWORD)-1)
		//Set timestamp
		frame.SetTimestamp(packet->GetTimestamp());
	//If not times
	if (frame.GetTime()==(QWORD)-1)
		//Set timestamp
		frame.SetTime(packet->GetTime());
	//Set SSRC
	frame.SetSSRC(packet->GetSSRC());
	//Add payload
	AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	//If it is last return frame
	return packet->GetMark() ? &frame : NULL;
}

MediaFrame* VP9Depacketizer::AddPayload(BYTE* payload, DWORD len)
{
	//Check length
	if (!len)
		//Exit
		return NULL;
    
	//Is first?
	int first = !frame.GetLength(); 
    
	VP9PayloadDescription desc;

	//Decode payload descriptr
	DWORD descLen = desc.Parse(payload,len);
	
	//Check
	if (!descLen || len<descLen)
	{
		//Error
		UltraDebug("-VP9Depacketizer::AddPayload() | Error decoding VP9 payload header\n");
		return NULL;
	}
	
	//Skip desc
	DWORD pos = frame.AppendMedia(payload+descLen, len-descLen);
	
	
	BYTE inmediate[14];
	
	//If it has the scalability structure
	if (desc.scalabiltiyStructureDataPresent)
	{
		//Get topmost spatial layer
		auto it = desc.scalabilityStructure.spatialLayerFrameResolutions.rbegin();
		//Ensure we have sizes
		if (it!=desc.scalabilityStructure.spatialLayerFrameResolutions.rend())
		{
			//Set sizes
			frame.SetWidth(it->first);
			frame.SetHeight(it->second);
		}
		//Disable it, as it can't be bigger than 14 buytes
		desc.scalabiltiyStructureDataPresent = false;
	}
	
	//Serialize it
	DWORD size = desc.Serialize(inmediate,sizeof(inmediate));
	
	//Add RTP packet
	frame.AddRtpPacket(pos,len-descLen,inmediate,size);
	
	//If it is first
	if (first)
		//calculate if it is an iframe
		frame.SetIntra(!desc.interPicturePredictedLayerFrame);
    
	return &frame;
}

