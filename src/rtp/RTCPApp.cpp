/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPApp.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:03
 */

#include "rtp/RTCPApp.h"
#include "rtp/RTCPCommonHeader.h"
#include "log.h"


RTCPApp::RTCPApp() : RTCPPacket(RTCPPacket::App)
{
}

RTCPApp::~RTCPApp()
{
	if (data)
		free(data);
}

DWORD RTCPApp::GetSize()
{
	return RTCPCommonHeader::GetSize()+8+size;
}

DWORD RTCPApp::Serialize(BYTE* data, DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPApp invalid size\n");
	
	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = subtype;
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	//Copy
	set4(data,len,ssrc);
	//Move
	len += 4;
	//Copy name
	memcpy(data+len,name,4);
	//Inc len
	len += 4;
	//Copy
	memcpy(data+len,this->data,this->size);
	//add it
	len += this->size;
	//return it
	return len;
}

DWORD RTCPApp::Parse(const BYTE* data,DWORD size)
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
	
	//Get subtype
	subtype = header.count;
	
	//Get ssrc
	ssrc = get4(data,len);
	//Move
	len += 4;
	//Copy name
	memcpy(name,data+len,4);
	//Skip
	len += 4;
	//Set size
	this->size = packetSize-len;
	//Allocate mem
	this->data = (BYTE*)malloc(this->size);
	//Copy data
	memcpy(this->data,data+len,this->size);
	//Skip
	len += this->size;
	//Copy
	return len;
}
