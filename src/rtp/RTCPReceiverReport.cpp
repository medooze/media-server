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

#include "rtp.h"


RTCPReceiverReport::RTCPReceiverReport() : RTCPPacket(RTCPPacket::ReceiverReport)
{

}

RTCPReceiverReport::~RTCPReceiverReport()
{
	for(Reports::iterator it = reports.begin();it!=reports.end();++it)
		delete(*it);
}

void RTCPReceiverReport::Dump()
{
	if (reports.size())
	{
		Debug("\t[RTCPReceiverReport ssrc=%u count=%u]\n",ssrc,reports.size());
		for(Reports::iterator it = reports.begin();it!=reports.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPReceiverReport]\n");
	} else
		Debug("\t[RTCPReceiverReport ssrc=%u]\n",ssrc);
}

DWORD RTCPReceiverReport::GetSize()
{
	return sizeof(rtcp_common_t)+4+24*reports.size();
}

DWORD RTCPReceiverReport::Parse(BYTE* data,DWORD size)
{
	//Get header
	rtcp_common_t * header = (rtcp_common_t *)data;

	//Check size
	if (size<GetRTCPHeaderLength(header))
		//Exit
		return 0;
	//Skip headder
	DWORD len = sizeof(rtcp_common_t);
	//Get info
	ssrc = get4(data,len);
	//Move forward
	len += 4;
	//for each
	for(int i=0;i<header->count&&size>=len+24;i++)
	{
		//New report
		RTCPReport* report = new RTCPReport();
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
	//Set header
	rtcp_common_t * header = (rtcp_common_t *)data;
	//Set values
	header->count	= reports.size();
	header->pt	= GetType();
	header->p	= 0;
	header->version = 2;
	SetRTCPHeaderLength(header,packetSize);
	//Skip
	DWORD len = sizeof(rtcp_common_t);
	//Set values
	set4(data,len,ssrc);
	//Next
	len += 4;
	//for each
	for(int i=0;i<header->count;i++)
		//Serialize
		len += reports[i]->Serialize(data+len,size-len);
	//return
	return len;
}
