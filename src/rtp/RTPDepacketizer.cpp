/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPDepacketizer.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:55
 */

#include "rtp.h"
#include "audio.h"
#include "bitstream.h"
#include "h264/h264depacketizer.h"
#include "vp8/vp8depacketizer.h"

class DummyAudioDepacketizer : public RTPDepacketizer
{
public:
	DummyAudioDepacketizer(DWORD codec) : RTPDepacketizer(MediaFrame::Audio,codec), frame((AudioCodec::Type)codec,8000)
	{

	}

	virtual ~DummyAudioDepacketizer()
	{

	}

	virtual void SetTimestamp(DWORD timestamp)
	{
		//Set timestamp
		frame.SetTimestamp(timestamp);
	}
	virtual MediaFrame* AddPacket(RTPPacket *packet)
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
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len)
	{
		//And data
		DWORD pos = frame.AppendMedia(payload, payload_len);
		//Add RTP packet
		frame.AddRtpPacket(pos,payload_len,NULL,0);
		//Return it
		return &frame;
	}
	virtual void ResetFrame()
	{
		//Clear packetization info
		frame.ClearRTPPacketizationInfo();
		//Reset
		memset(frame.GetData(),0,frame.GetMaxMediaLength());
		//Clear length
		frame.SetLength(0);
	}
	virtual DWORD GetTimestamp() 
	{
		return frame.GetTimeStamp();
	}
private:
	AudioFrame frame;
};

RTPDepacketizer* RTPDepacketizer::Create(MediaFrame::Type mediaType,DWORD codec)
{
	 switch (mediaType)
	 {
		 case MediaFrame::Video:
			 //Depending on the codec
			 switch((VideoCodec::Type)codec)
			 {
				 case VideoCodec::H264:
					 return new H264Depacketizer();
				 case VideoCodec::VP8:
					 return new VP8Depacketizer();                
			 }
			 break;
		 case MediaFrame::Audio:
			 //Dummy depacketizer
			 return new DummyAudioDepacketizer(codec);
			 break;
	 }
	 return NULL;
}