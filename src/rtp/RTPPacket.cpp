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

#include "rtp/RTPPacket.h"
#include "log.h"


RTPPacket::RTPPacket(MediaFrame::Type media,BYTE codec)
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
	//Reset payload
	payload  = buffer+PREFIX;
	payloadLen = 0;
	//Set time
	time = ::getTimeMS();
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
	//Reset payload
	payload  = buffer+PREFIX;
	payloadLen = 0;
	//Set time
	time = ::getTimeMS();
}

RTPPacket::~RTPPacket()
{
}

RTPPacket::shared RTPPacket::Clone()
{
	//New one
	auto cloned = std::make_shared<RTPPacket>(GetMedia(),GetCodec(),GetRTPHeader(),GetRTPHeaderExtension());
	//Set attrributes
	cloned->SetClockRate(GetClockRate());
	cloned->SetSeqCycles(GetSeqCycles());
	cloned->SetTimestamp(GetTimestamp());
	cloned->SetTime(GetTime());
	//Set payload
	cloned->SetPayload(GetMediaData(),GetMediaLength());
	//Return it
	return cloned;
}

DWORD RTPPacket::Serialize(BYTE* data,DWORD size,const RTPMap& extMap) const
{
	//Serialize header
	int len = header.Serialize(data,size);

	//Check
	if (!len)
		//Error
		return Error("-RTPPacket::Serialize() | Error serializing rtp headers\n");

	//If we have extension
	if (header.extension)
	{
		//Serialize
		int n = extension.Serialize(extMap,data+len,size-len);
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

	//Copy media payload
	memcpy(data+len,GetMediaData(),GetMediaLength());
	
	//Inc payload len
	len += GetMediaLength();
	
	//Return copied len
	return len;
}

bool RTPPacket::SetPayload(BYTE *data,DWORD size)
{
	//Check size
	if (size>GetMaxMediaLength())
		//Error
		return false;
	//Reset payload
	payload  = buffer+PREFIX;
	//Copy
	memcpy(payload,data,size);
	//Set length
	payloadLen = size;
	//good
	return true;
}
bool RTPPacket::PrefixPayload(BYTE *data,DWORD size)
{
	//Check size
	if (size>payload-buffer)
		//Error
		return false;
	//Copy
	memcpy(payload-size,data,size);
	//Set pointers
	payload  -= size;
	payloadLen += size;
	//good
	return true;
}


bool RTPPacket::SkipPayload(DWORD skip) 
{
	//Ensure we have enough to skip
	if (GetMaxMediaLength()<skip+(payload-buffer)+payloadLen)
		//Error
		return false;

	//Move media data
	payload += skip;
	//Set length
	payloadLen -= skip;
	//good
	return true;
}

void RTPPacket::Dump()
{
	Debug("[RTPPacket %s codec=%u payload=%d]\n",MediaFrame::TypeToString(GetMedia()),GetCodec(),GetMediaLength());
	header.Dump();
	//If  there is an extension
	if (header.extension)
		//Dump extension
		extension.Dump();
	::Dump(GetMediaData(),16);
	Debug("[/RTPPacket]\n");
}