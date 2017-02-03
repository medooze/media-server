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

#include "rtp.h"


RTCPExtendedJitterReport::RTCPExtendedJitterReport() : RTCPPacket(RTCPPacket::ExtendedJitterReport)
{
}

DWORD RTCPExtendedJitterReport::GetSize()
{
	return sizeof(rtcp_common_t)+4*jitters.size();
}

DWORD RTCPExtendedJitterReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<header->count;i++)
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
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= jitters.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<jitters.size();i++)
	{
		//Set ssrc
		set4(data,len,jitters[i]);
		//skip
		len += 4;
	}
	//return
	return len;
}