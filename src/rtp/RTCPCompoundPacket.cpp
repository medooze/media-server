/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPCompoundPacket.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:04
 */

#include "rtp/RTCPCompoundPacket.h"
#include "rtp/RTCPReceiverReport.h"
#include "rtp/RTCPSenderReport.h"
#include "rtp/RTCPSDES.h"
#include "rtp/RTCPBye.h"
#include "rtp/RTCPApp.h"
#include "rtp/RTCPRTPFeedback.h"
#include "rtp/RTCPPayloadFeedback.h"
#include "rtp/RTCPPayloadFeedback.h"
#include "rtp/RTCPFullIntraRequest.h"
#include "rtp/RTCPNACK.h"
#include "rtp/RTCPExtendedJitterReport.h"

RTCPCompoundPacket::~RTCPCompoundPacket()
{
	//Fir each oen
	for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
		//delete
		delete((*it));
}

DWORD RTCPCompoundPacket::RTCPCompoundPacket::GetSize() const
{
	DWORD size = 0;
	//Calculate
	for(RTCPPackets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
		//Append size
		size = RTCPCommonHeader::GetSize()+(*it)->GetSize();
	//Return total size
	return size;
}

DWORD RTCPCompoundPacket::Serialize(BYTE *data,DWORD size) const
{
	DWORD len = 0;
	//Check size
	if (size<GetSize())
		//Error
		return 0;
	//For each one
	for(RTCPPackets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
		//Serialize
		len +=(*it)->Serialize(data+len,size-len);
	//Exit
	return len;
}
	

RTCPCompoundPacket* RTCPCompoundPacket::Parse(BYTE *data,DWORD size)
{
	//Check if it is an RTCP valid header
	if (!IsRTCP(data,size))
	{
		Error("not rtcp packet");
		//Exit
		return NULL;
	}
	//Create pacekt
	RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();
	//Init pointers
	BYTE *buffer = data;
	DWORD bufferLen = size;
	//Parse
	while (bufferLen)
	{
		RTCPPacket *packet = NULL;
		RTCPCommonHeader header;
		//Get type from header
		DWORD len = header.Parse(data,size);
		//If not parsed
		if (!len)
		{
			//error
			Error("Wrong rtcp header\n");
			//Exit
			return NULL;
		}
		//Check len
		if (header.length>bufferLen)
		{
			//error
			Error("Wrong rtcp packet size\n");
			//Exit
			return NULL;
		}
		//Create new packet
		switch (header.packetType)
		{
			case RTCPPacket::SenderReport:
				//Create packet
				packet = new RTCPSenderReport();
				break;
			case RTCPPacket::ReceiverReport:
				//Create packet
				packet = new RTCPReceiverReport();
				break;
			case RTCPPacket::SDES:
				//Create packet
				packet = new RTCPSDES();
				break;
			case RTCPPacket::Bye:
				//Create packet
				packet = new RTCPBye();
				break;
			case RTCPPacket::App:
				//Create packet
				packet = new RTCPApp();
				break;
			case RTCPPacket::RTPFeedback:
				//Create packet
				packet = new RTCPRTPFeedback();
				break;
			case RTCPPacket::PayloadFeedback:
				//Create packet
				packet = new RTCPPayloadFeedback();
				break;
			case RTCPPacket::FullIntraRequest:
				//Create packet
				packet = new RTCPFullIntraRequest();
				break;
			case RTCPPacket::NACK:
				//Create packet
				packet = new RTCPNACK();
				break;
			case RTCPPacket::ExtendedJitterReport:
				//Create packet
				packet = new RTCPExtendedJitterReport();
				break;
			default:
				//Skip
				Debug("Unknown rtcp packet type [%d]\n",header.packetType);
		}

		//parse
		if (packet && packet->Parse(buffer,header.length))
			//Add packet
			rtcp->AddRTCPacket(packet);
		//Remove size
		bufferLen -= header.length;
		//Increase pointer
		buffer    += header.length;
	}

	//Return it
	return rtcp;
}

void RTCPCompoundPacket::Dump() const
{
	Debug("[RTCPCompoundPacket count=%d size=%d]\n",packets.size(),GetSize());
	//For each one
	for(RTCPPackets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
		//Dump
		(*it)->Dump();
	Debug("[/RTCPCompoundPacket]\n");
}