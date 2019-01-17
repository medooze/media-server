/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPExtendedJitterReport.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:01
 */

#include "rtp/RTCPExtendedJitterReport.h"
#include "rtp/RTCPCommonHeader.h"
#include "log.h"

RTCPExtendedJitterReport::RTCPExtendedJitterReport() : RTCPPacket(RTCPPacket::ExtendedJitterReport)
{
}

DWORD RTCPExtendedJitterReport::GetSize()
{
	return RTCPCommonHeader::GetSize()+4*jitters.size();
}

DWORD RTCPExtendedJitterReport::Parse(const BYTE* data,DWORD size)
{
	//Get header
	RTCPCommonHeader header;
		
	//Parse header
	DWORD len = header.Parse(data,size);
	
	//IF error
	if (!len)
		return 0;
		
	//Get packet size
	DWORD packetSize = header.length;
	
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	
	//for each
	for(DWORD i=0;i<header.count;i++)
	{
		//Get ssrc
		jitters.push_back(get4(data,len));
		//Add lenght
		len+=4;
	}

	//Return total size
	return len;
}

DWORD RTCPExtendedJitterReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPExtendedJitterReport invalid size\n");

	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = jitters.size();
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	//for each
	for(DWORD i=0;i<jitters.size();i++)
	{
		//Set ssrc
		set4(data,len,jitters[i]);
		//skip
		len += 4;
	}
	//return
	return len;
}