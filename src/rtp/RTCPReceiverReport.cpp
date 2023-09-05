/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPReceiverReport.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:00
 */

#include "rtp/RTCPReceiverReport.h"
#include "rtp/RTCPCommonHeader.h"

RTCPReceiverReport::RTCPReceiverReport(DWORD ssrc) : RTCPPacket(RTCPPacket::ReceiverReport)
{
	this->ssrc = ssrc;
}

RTCPReceiverReport::RTCPReceiverReport()  : RTCPPacket(RTCPPacket::ReceiverReport)
{
}


void RTCPReceiverReport::Dump()
{
	if (reports.size())
	{
		Debug("\t[RTCPReceiverReport ssrc=%u count=%llu]\n",ssrc,reports.size());
		for(Reports::iterator it = reports.begin();it!=reports.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPReceiverReport]\n");
	} else
		Debug("\t[RTCPReceiverReport ssrc=%u]\n",ssrc);
}

DWORD RTCPReceiverReport::GetSize()
{
	return RTCPCommonHeader::GetSize()+4+24*reports.size();
}

DWORD RTCPReceiverReport::Parse(const BYTE* data,DWORD size)
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
	
	//Get info
	ssrc = get4(data,len);
	//Move forward
	len += 4;
	//for each
	for(int i=0;i<header.count&&size>=len+24;i++)
	{
		//New report
		auto report = std::make_shared<RTCPReport>();
		//parse
		len += report->Parse(data+len,size-len);
		//Add it
		AddReport(report);
	}

	//Return total size
	return len;
}

DWORD RTCPReceiverReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPReceiverReport invalid size\n");

	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = reports.size();
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	set4(data,len,ssrc);
	//Next
	len += 4;
	//for each
	for(int i=0;i<header.count;i++)
		//Serialize
		len += reports[i]->Serialize(data+len,size-len);
	//return
	return len;
}
