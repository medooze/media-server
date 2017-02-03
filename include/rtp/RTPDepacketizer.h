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
	virtual MediaFrame* AddPacket(RTPPacket *packet) = 0;
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len) = 0;
	virtual void ResetFrame() = 0;
	virtual DWORD GetTimestamp() = 0;
private:
	MediaFrame::Type	mediaType;
	DWORD			codec;
};
#endif /* RTPDEPACKETIZER_H */

