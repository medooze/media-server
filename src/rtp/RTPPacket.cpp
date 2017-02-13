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


RTPPacket::RTPPacket(MediaFrame::Type media,DWORD codec)
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
			clockRate = 8000;
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
RTPPacket::RTPPacket(MediaFrame::Type media,DWORD codec,const RTPHeader &header, const RTPHeaderExtension extension) :
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
			clockRate = 8000;
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

RTPPacket* RTPPacket::Clone()
{
	//New one
	RTPPacket* cloned = new RTPPacket(GetMedia(),GetCodec(),GetRTPHeader(),GetRTPHeaderExtension());
	//Set attrributes
	cloned->SetClockRate(GetClockRate());
	cloned->SetSeqCycles(GetSeqCycles());
	cloned->SetTimestamp(GetTimestamp());
	//Set payload
	cloned->SetPayload(GetMediaData(),GetMediaLength());
	//Return it
	return cloned;
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


bool RTPPacket::SkipPayload(DWORD skip) {
	//Ensure we have enough to skip
	if (GetMaxMediaLength()<skip+(payload-buffer)+payloadLen)
		//Error
		return false;

	//Move media data
	payload += skip;
	//good
	return true;
}

void RTPPacket::Dump()
{
	Debug("[RTPPacket %s codec=%d payload=%d]\n",MediaFrame::TypeToString(GetMedia()),GetCodec(),GetMediaLength());
	header.Dump();
	//If  there is an extension
	if (header.extension)
		//Dump extension
		extension.Dump();
	::Dump(GetMediaData(),16);
	Log("[[/RTPPacket]\n");
}