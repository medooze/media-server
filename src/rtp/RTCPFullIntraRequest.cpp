/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPFullIntraRequest.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:01
 */

#include "rtp.h"



RTCPFullIntraRequest::RTCPFullIntraRequest() : RTCPPacket(RTCPPacket::FullIntraRequest)
{

}

DWORD RTCPFullIntraRequest::GetSize()
{
	return sizeof(rtcp_common_t)+4;
}

DWORD RTCPFullIntraRequest::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get ssrcs
	ssrc = get4(data,len);
	//Return consumed len
	return len+4;
}

DWORD RTCPFullIntraRequest::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPFullIntraRequest invalid size\n");
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= 0;
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Set lenght
	DWORD len = sizeof(rtcp_common_t);
	//Set ssrcs
	set4(data,len,ssrc);
	//Retrun writed data len
	return len+4;
}
