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

#include "rtp/RTCPBye.h"
#include "rtp/RTCPCommonHeader.h"



RTCPBye::RTCPBye() : RTCPPacket(RTCPPacket::Bye)
{
}

RTCPBye::RTCPBye(const std::vector<DWORD> &ssrcs,const char* reason) : 
	RTCPPacket(RTCPPacket::Bye) ,
	ssrcs(ssrcs)
{
	if (reason)
		//Store reason
		this->reason = strdup(reason);
	else
		this->reason = nullptr;
}

RTCPBye::~RTCPBye()
{
	//Free
	if (reason)
		free(reason);
}
DWORD RTCPBye::GetSize()
{
	DWORD len = RTCPCommonHeader::GetSize()+4*ssrcs.size();
	if (reason)
		len += strlen(reason)+1;
	//Return
	return pad32(len);;
}

DWORD RTCPBye::Parse(const BYTE* data,DWORD size)
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
	for(BYTE i=0;i<header.count;i++)
	{
		//Get ssrc
		ssrcs.push_back(get4(data,len));
		//Add lenght
		len+=4;
	}

	//Check if more present
	if (packetSize>len)
	{
		//Get len for reason
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

	//Skip padding
	return pad32(len);
}

DWORD RTCPBye::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPBye invalid size\n");

	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = ssrcs.size();
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	//for each
	for(DWORD i=0;i<ssrcs.size();i++)
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
		data[len++] = strlen(reason);
		//Copy reason
		memcpy(data+len,reason,strlen(reason));
		//Add len
		len += strlen(reason);
	}

	//Append nulls till pading
	memset(data+len,0,pad32(len)-len);
	
	//return
	return pad32(len);
}


void RTCPBye::Dump()
{
	Debug("\t[RTCPBye size=%d reason=%s]\n", GetSize(), reason ? reason : "(null)");
	for(DWORD i=0;i<ssrcs.size();i++)
		Debug("\t\t[ssrc=%u/]\n",ssrcs[i]);
	Debug("\t[RTCPBye/]\n");
		
}