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
#include "rtp/RTCPNACK.h"
#include "rtp/RTCPCommonHeader.h"

RTCPNACK::RTCPNACK() : RTCPPacket(RTCPPacket::NACK)
{

}

DWORD RTCPNACK::GetSize()
{
	return RTCPCommonHeader::GetSize()+8;
}

DWORD RTCPNACK::Parse(const BYTE* data,DWORD size)
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

	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = 0;
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	//Set ssrcs
	set4(data,len,ssrc);
	set2(data,len+4,fsn);
	set2(data,len+6,blp);
	//Retrun writed data len
	return len+8;
}
