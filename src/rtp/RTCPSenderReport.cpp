/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPSenderReport.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:59
 */

#include "rtp/RTCPSenderReport.h"
#include "rtp/RTCPCommonHeader.h"
#include "log.h"

RTCPSenderReport::RTCPSenderReport() : RTCPPacket(RTCPPacket::SenderReport)
{
}

void RTCPSenderReport::SetTimestamp(QWORD time)
{
	/*
	   Wallclock time (absolute date and time) is represented using the
	   timestamp format of the Network Time Protocol (NTP), which is in
	   seconds relative to 0h UTC on 1 January 1900 [4].  The full
	   resolution NTP timestamp is a 64-bit unsigned fixed-point number with
	   the integer part in the first 32 bits and the fractional part in the
	   last 32 bits.  In some fields where a more compact representation is
	   appropriate, only the middle 32 bits are used; that is, the low 16
	   bits of the integer part and the high 16 bits of the fractional part.
	   The high 16 bits of the integer part must be determined
	   independently.
	 */
	//Get seconds
	DWORD sec = time/1E6;
	DWORD usec = time-sec*1E6;
	
	//Convert from ecpoch (JAN_1970) to NTP (JAN 1900);
	SetNTPSec(sec + 2208988800UL);
	//Convert microsecods to 32 bits fraction
	SetNTPFrac(usec*4294.967296);
}

QWORD RTCPSenderReport::GetTimestamp() const
{
	//Convert to epcoh JAN_1970
	QWORD ts = ntpSec - 2208988800UL;
	//convert to microseconds
	ts *=1E6;
	//Add fraction
	ts += ntpFrac/4294.967296;
	//Return it
	return ts;
}

void RTCPSenderReport::Dump()
{
	Debug("\t[RTCPSenderReport ssrc=%u count=%llu \n",ssrc,reports.size());
	Debug("\t\tntpSec=%u\n"		,ntpSec);
	Debug("\t\tntpFrac=%u\n"	,ntpFrac);
	Debug("\t\trtpTimestamp=%u\n"	,rtpTimestamp);
	Debug("\t\tpacketsSent=%u\n"	,packetsSent);
	Debug("\t\toctectsSent=%u\n"	,octectsSent);
	if (reports.size())
	{
		Debug("\t]\n");
		for(Reports::iterator it = reports.begin();it!=reports.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPSenderReport]\n");
	} else
		Debug("\t/]\n");
}

DWORD RTCPSenderReport::GetSize()
{
	return RTCPCommonHeader::GetSize()+24+24*reports.size();
}

DWORD RTCPSenderReport::Parse(const BYTE* data,DWORD size)
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
	ssrc		= get4(data,len);
	ntpSec		= get4(data,len+4);
	ntpFrac		= get4(data,len+8);
	rtpTimestamp	= get4(data,len+12);
	packetsSent	= get4(data,len+16);
	octectsSent	= get4(data,len+20);
	//Move forward
	len += 24;
	//for each
	for(int i=0;i<header.count;i++)
	{
		//New report
		auto report = std::make_shared<RTCPReport>();
		//parse
		DWORD l = report->Parse(data+len,size-len);
		//If not parsed correctly
		if (!l)
			return 0;
		//Add it
		AddReport(report);
		//INcrease len
		len += l;
	}
	
	//Return total size
	return len;
}

DWORD RTCPSenderReport::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSenderReport invalid size\n");

	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = reports.size();
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	//Set values
	set4(data,len,ssrc);
	set4(data,len+4,ntpSec);
	set4(data,len+8,ntpFrac);
	set4(data,len+12,rtpTimestamp);
	set4(data,len+16,packetsSent);
	set4(data,len+20,octectsSent);
	//Next
	len += 24;
	//for each
	for(int i=0;i<header.count;i++)
		//Serialize
		len += reports[i]->Serialize(data+len,size-len);
	//return
	return len;
}
