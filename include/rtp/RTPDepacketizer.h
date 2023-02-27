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

	MediaFrame::Type GetMediaType()	const { return mediaType; }
	DWORD		 GetCodec()	const { return codec; }

	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) = 0;
	virtual MediaFrame* AddPayload(const BYTE* payload,DWORD payload_len) = 0;
	virtual void ResetFrame() = 0;
private:
	MediaFrame::Type	mediaType;
	DWORD			codec;
};


class DummyAudioDepacketizer : public RTPDepacketizer
{
public:
	DummyAudioDepacketizer(AudioCodec::Type codec, DWORD rate) : RTPDepacketizer(MediaFrame::Audio,codec), frame(codec)
	{
		frame.SetClockRate(rate);
	}
	
	DummyAudioDepacketizer(AudioCodec::Type codec) : RTPDepacketizer(MediaFrame::Audio,codec), frame(codec)
	{
		frame.SetClockRate(8000);
	}

	virtual ~DummyAudioDepacketizer()
	{

	}

	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet)
	{
		//Reset frame
		ResetFrame();
		//Set timestamp
		frame.SetTimestamp(packet->GetExtTimestamp());
		//Set clock rate
		frame.SetClockRate(packet->GetClockRate());
		//Set time
		frame.SetTime(packet->GetTime());
		//Set sender time
		frame.SetSenderTime(packet->GetSenderTime());
		//Set timestamp skew
		frame.SetTimestampSkew(packet->GetTimestampSkew());
		//Set SSRC
		frame.SetSSRC(packet->GetSSRC());
		//Add payload
		AddPayload(packet->GetMediaData(),packet->GetMediaLength());
		//Return frame
		return &frame;
	}
	virtual MediaFrame* AddPayload(const BYTE* payload,DWORD payload_len)
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
		//Reset frame data
		frame.Reset();
	}

private:
	AudioFrame frame;
};

#endif /* RTPDEPACKETIZER_H */

