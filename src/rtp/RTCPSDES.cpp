/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPSDES.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:02
 */

#include "rtp/RTCPSDES.h"
#include "rtp/RTCPCommonHeader.h"

RTCPSDES::RTCPSDES() : RTCPPacket(RTCPPacket::SDES)
{

}

void RTCPSDES::Dump()
{
	
	if (descriptions.size())
	{
		Debug("\t[RTCPSDES count=%llu]\n",descriptions.size());
		for(Descriptions::iterator it = descriptions.begin();it!=descriptions.end();++it)
			(*it)->Dump();
		Debug("\t[/RTCPSDES]\n");
	} else
		Debug("\t[RTCPSDES/]\n");
}
DWORD RTCPSDES::GetSize()
{
	DWORD len = RTCPCommonHeader::GetSize();
	//For each field
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}
DWORD RTCPSDES::Parse(const BYTE* data,DWORD size)
{
	//Get header
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
	
	//Parse fields
	DWORD i = 0;
	//While we have
	while (size>len && i<header.count)
	{
		auto desc = std::make_shared<Description>();
		//Parse field
		DWORD parsed = desc->Parse(data+len,size-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		descriptions.push_back(desc);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len;
}

DWORD RTCPSDES::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSDES invalid size\n");

	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = descriptions.size();
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	//For each field
	for (Descriptions::iterator it=descriptions.begin();it!=descriptions.end();++it)
		//Serilize it
		len += (*it)->Serialize(data+len,size-len);

	//Return
	return len;
 }


RTCPSDES::Description::Description()
{

}
RTCPSDES::Description::Description(DWORD ssrc)
{
	this->ssrc = ssrc;
}

void RTCPSDES::Description::Dump()
{
	if (items.size())
	{
		Debug("\t\t[Description ssrc=%u count=%llu\n",ssrc,items.size());
		for(Items::iterator it=items.begin();it!=items.end();++it)
			Debug("\t\t\t[%s '%.*s'/]\n",RTCPSDES::Item::TypeToString((*it)->GetType()),(*it)->GetSize(),(*it)->GetData());
		Debug("\t\t[/Description]\n");
	} else
		Debug("\t\t[Description ssrc=%u/]\n",ssrc);
}

DWORD RTCPSDES::Description::GetSize()
{
	DWORD len = 4;
	//For each field
	for (Items::iterator it=items.begin();it!=items.end();++it)
		//add data size and header
		len += (*it)->GetSize()+2;
	//ADD end
	len+=1;
	//Return
	return pad32(len);
}
DWORD RTCPSDES::Description::Parse(const BYTE* data,DWORD size)
{
	//Check size
	if (size<4)
		//Exit
		return 0;
	//Get ssrc
	ssrc = get4(data,0);
	//Skip ssrc
	DWORD len = 4;
	//While we have
	while (size>len+2 && data[len])
	{
		//Get tvalues
		RTCPSDES::Item::Type type = (RTCPSDES::Item::Type)data[len];
		BYTE length = data[len+1];
		//Check size
		if (len+2+length>size)
			//Error
			return 0;
		//Add item
		items.push_back(std::make_shared<Item>(type,data+len+2,length));
		//Move
		len += length+2;
	}
	//Skip last
	len++;
	//Return consumed len
	return pad32(len);
}

DWORD RTCPSDES::Description::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPSDES Description invalid size\n");
	//Set ssrc
	set4(data,0,ssrc);
	//Skip headder
	DWORD len = 4;
	//For each item
	for (auto item : items)
	{
		//Serilize it
		data[len++] = item->GetType();
		data[len++] = item->GetSize();
		//Copy data
		memcpy(data+len,item->GetData(),item->GetSize());
		//Move
		len += item->GetSize();
	}
	//Add null item
	data[len++] = 0;
	//Append nulls till pading
	memset(data+len,0,pad32(len)-len);
	//Return padded size
	return pad32(len);
 }