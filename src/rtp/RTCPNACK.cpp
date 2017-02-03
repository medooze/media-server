/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPNACK.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:02
 */
#include "rtp.h"


RTCPNACK::RTCPNACK() : RTCPPacket(RTCPPacket::NACK)
{

}

DWORD RTCPNACK::GetSize()
{
	return sizeof(rtcp_common_t)+8;
}

DWORD RTCPNACK::Parse(BYTE* data,DWORD size)
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
	fsn = get2(data,len+4);
	blp = get2(data,len+2);
	//Return consumed len
	return len+8;
}

DWORD RTCPNACK::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPNACK invalid size\n");
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
	set2(data,len+4,fsn);
	set2(data,len+6,blp);
	//Retrun writed data len
	return len+8;
}
