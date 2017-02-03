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

#include "rtp.h"



RTCPApp::RTCPApp() : RTCPPacket(RTCPPacket::App)
{
	data = NULL;
	size = 0;
	subtype = 0;
}

RTCPApp::~RTCPApp()
{
	if (data)
		free(data);
}

DWORD RTCPApp::GetSize()
{
	return sizeof(rtcp_common_t)+8+size;
}

DWORD RTCPApp::Serialize(BYTE* data, DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPApp invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= subtype;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
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

DWORD RTCPApp::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Get packet size
	DWORD packetSize = GetRTCPHeaderLength(header);
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	//Get subtype
	subtype = header->count;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
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
