/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPBye.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:00
 */

#include "rtp.h"




RTCPBye::RTCPBye() : RTCPPacket(RTCPPacket::Bye)
{
	//No reason
	reason = NULL;
}

RTCPBye::~RTCPBye()
{
	//Free
	if (reason)
		free(reason);
}
DWORD RTCPBye::GetSize()
{
	DWORD len = sizeof(rtcp_common_t)+4*ssrcs.size();
	if (reason)
		len += strlen(reason)+1;
	return len;
}

DWORD RTCPBye::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<header->count;i++)
	{
		//Get ssrc
		ssrcs.push_back(get4(data,len));
		//Add lenght
		len+=4;
	}

	//Check if more preseng
	if (packetSize>len)
	{
		//Get len or reason
		DWORD n = data[len];
		//Allocate mem
		reason = (char*)malloc(n+1);
		//Copy
		memcpy(reason,data+len+1,n);
		//End it
		reason[n] = 0;
		//Move
		len += n+1;
	}

	//Return total size
	return len;
}

DWORD RTCPBye::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPBye invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= ssrcs.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//for each
	for(int i=0;i<ssrcs.size();i++)
	{
		//Set ssrc
		set4(data,len,ssrcs[i]);
		//skip
		len += 4;
	}
	//Optional reason
	if (reason)
	{
		//Set reason length
		data[len] = strlen(reason);

		//Copy reason
		memcpy(data+len+1,reason,strlen(reason));

		//Add len
		len +=strlen(reason)+1;
	}

	//return
	return len;
}

