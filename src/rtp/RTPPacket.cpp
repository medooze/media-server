/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPPacket.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:52
 */

#include <array>

#include "rtp/RTPPacket.h"
#include "log.h"

RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec, QWORD time) 
{
	this->media = media;
	//Set codec
	this->codec = codec;
	//NO seq cycles
	cycles = 0;
	//Default clock rates
	switch(media)
	{
		case MediaFrame::Video:
			clockRate = 90000;
			break;
		case MediaFrame::Audio:
			clockRate = AudioCodec::GetClockRate((AudioCodec::Type)codec);
			break;
		default:
			clockRate = 1000;
	}
	//Create payload
	payload  = std::make_shared<RTPPayload>();
	//We own the payload
	ownedPayload = true;
	//Set time
	this->time = time;
}

RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec) : RTPPacket(media, codec, ::getTimeMS())
{
}

RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec,const RTPHeader &header, const RTPHeaderExtension &extension) :
	header(header),
	extension(extension)
{
	this->media = media;
	//Set coced
	this->codec = codec;
	//NO seq cycles
	cycles = 0;
	//Default clock rates
	switch(media)
	{
		case MediaFrame::Video:
			clockRate = 90000;
			break;
		case MediaFrame::Audio:
			clockRate = AudioCodec::GetClockRate((AudioCodec::Type)codec);
			break;
		default:
			clockRate = 1000;
	}
	//Create payload
	payload  = std::make_shared<RTPPayload>();
	//We own the payload
	ownedPayload = true;
	//Set time
	this->time = ::getTimeMS();
}


RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec,const RTPHeader &header, const RTPHeaderExtension &extension, const RTPPayload::shared &payload, QWORD time) :
	header(header),
	extension(extension)
{
	this->media = media;
	//Set coced
	this->codec = codec;
	//NO seq cycles
	cycles = 0;
	//Default clock rates
	switch(media)
	{
		case MediaFrame::Video:
			clockRate = 90000;
			break;
		case MediaFrame::Audio:
			clockRate = AudioCodec::GetClockRate((AudioCodec::Type)codec);
			break;
		default:
			clockRate = 1000;
	}
	//Copy payload object
	this->payload  = payload;
	//Owned payload
	ownedPayload = false;
	//Set time
	this->time = time;
}

RTPPacket::~RTPPacket()
{
}

RTPPacket::shared RTPPacket::Clone()
{
	//New one
	auto cloned = std::make_shared<RTPPacket>(GetMedia(),GetCodec(),GetRTPHeader(),GetRTPHeaderExtension(),payload,GetTime());
	//Set attrributes
	cloned->SetClockRate(GetClockRate());
	cloned->SetSeqCycles(GetSeqCycles());
	cloned->SetTimestamp(GetTimestamp());
	//Copy descriptors
	cloned->vp8PayloadDescriptor = vp8PayloadDescriptor;
	cloned->vp8PayloadHeader     = vp8PayloadHeader;
	cloned->vp9PayloadDescriptor = vp9PayloadDescriptor;
	//Return it
	return cloned;
}

DWORD RTPPacket::Serialize(BYTE* data,DWORD size,const RTPMap& extMap) const
{
	//Serialize header
	uint32_t len = header.Serialize(data,size);

	//Check
	if (!len)
		//Error
		return Error("-RTPPacket::Serialize() | Error serializing rtp headers\n");

	//If we have extension
	if (header.extension)
	{
		//Serialize
		uint32_t n = extension.Serialize(extMap,data+len,size-len);
		//Comprobamos que quepan
		if (!n)
			//Error
			return Error("-RTPPacket::Serialize() | Error serializing rtp extension headers\n");
		//Inc len
		len += n;
	}

	//Ensure we have enougth data
	if (len+GetMediaLength()>size)
		//Error
		return Error("-RTPPacket::Serialize() | Media overflow\n");
	
	//If we need to rewrite the vp8 descriptor
	if (rewitePictureIds && vp8PayloadDescriptor)
	{
		//Get current vp8 descriptor length
		uint32_t descLen = vp8PayloadDescriptor->GetSize();
		//Copy the descriptor
		auto vp8NewPayloadDescriptor = vp8PayloadDescriptor.value();
		//Always store it as two bytes
		vp8NewPayloadDescriptor.pictureIdPresent = 1;
		vp8NewPayloadDescriptor.pictureIdLength = 2;
		//Write it back
		len += vp8NewPayloadDescriptor.Serialize(data+len,size-len);
		
		//Check size
		if (descLen>GetMediaLength())
			//Error
			return Error("-RTPPacket::Serialize() | Wrong vp8PayloadDescriptor when rewriting pict ids\n");
		
		//Copy media payload without the old description
		memcpy(data+len,GetMediaData()+descLen,GetMediaLength()-descLen);
		//Inc payload len
		len += GetMediaLength()-descLen;
		
	} else {
		//Copy media payload
		memcpy(data+len,GetMediaData(),GetMediaLength());
		//Inc payload len
		len += GetMediaLength();
	}
	
	
	//Return copied len
	return len;
}

BYTE* RTPPacket::AdquireMediaData()
{
	//If the packet was cloned and doesn't own the payload
	if (!ownedPayload)
	{
		//Clone payload
		payload = payload->Clone();
		//We own the payload
		ownedPayload = true;
	}
	//You can write on payload now
	return payload->GetPayloadData();
}

bool RTPPacket::RecoverOSN()
{
	/*
	The format of a retransmission packet is shown below:
	 0                   1                   2                   3
	 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                         RTP Header                            |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|            OSN                |                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
	|                  Original RTP Packet Payload                  |
	|                                                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	//Ensure we have enought data
	if (GetMediaLength()<2)
		//error
	       return false;
	
	//Get original sequence number
	WORD osn = get2(GetMediaData(),0);
	
	 //Skip osn from payload
	if (!SkipPayload(2))
		//error
		return false;

	//Set original seq num
	SetSeqNum(osn);
	
	//Done
	return true;
}

void RTPPacket::Dump()
{
	Debug("[RTPPacket %s codec=%s payload=%d extSeqNum=%u(%u)]\n",MediaFrame::TypeToString(GetMedia()),GetNameForCodec(GetMedia(),GetCodec()),GetMediaLength(),GetExtSeqNum(),GetSeqCycles());
	header.Dump();
	//If  there is an extension
	if (header.extension)
		//Dump extension
		extension.Dump();
	if (vp8PayloadDescriptor)
		vp8PayloadDescriptor->Dump();
	if (vp8PayloadHeader)
		vp8PayloadHeader->Dump();
	::Dump(GetMediaData(),16);
	Debug("[/RTPPacket]\n");
}
