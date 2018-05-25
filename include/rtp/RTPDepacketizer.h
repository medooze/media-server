/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPDepacketizer.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:55
 */

#ifndef RTPDEPACKETIZER_H
#define RTPDEPACKETIZER_H
#include "config.h"
#include "media.h"
#include "audio.h"
#include "rtp/RTPPacket.h"
class RTPDepacketizer
{
public:
	static RTPDepacketizer* Create(MediaFrame::Type mediaType,DWORD codec);

public:
	RTPDepacketizer(MediaFrame::Type mediaType,DWORD codec)
	{
		//Store
		this->mediaType = mediaType;
		this->codec = codec;
	}
	virtual ~RTPDepacketizer()	{};

	MediaFrame::Type GetMediaType()	{ return mediaType; }
	DWORD		 GetCodec()	{ return codec; }

	virtual void SetTimestamp(DWORD timestamp) = 0;
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) = 0;
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len) = 0;
	virtual void ResetFrame() = 0;
	virtual DWORD GetTimestamp() = 0;
private:
	MediaFrame::Type	mediaType;
	DWORD			codec;
};


class DummyAudioDepacketizer : public RTPDepacketizer
{
public:
	DummyAudioDepacketizer(DWORD codec, DWORD rate) : RTPDepacketizer(MediaFrame::Audio,codec), frame((AudioCodec::Type)codec,rate)
	{

	}
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
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet)
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
		//Return frame
		return &frame;
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
		//Clear time
		frame.SetTimestamp((DWORD)-1);
		frame.SetTime((QWORD)-1);
	}
	virtual DWORD GetTimestamp() 
	{
		return frame.GetTimeStamp();
	}
private:
	AudioFrame frame;
};

#endif /* RTPDEPACKETIZER_H */

