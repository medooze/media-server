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

#include "rtp.h"

void RTCPCompoundPacket::Dump()
{
	Debug("[RTCPCompoundPacket count=%d size=%d]\n",packets.size(),GetSize());
	//For each one
	for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
		//Dump
		(*it)->Dump();
	Debug("[/RTCPCompoundPacket]\n");
}
RTCPCompoundPacket* RTCPCompoundPacket::Parse(BYTE *data,DWORD size)
{
	//Check if it is an RTCP valid header
	if (!IsRTCP(data,size))
		//Exit
		return NULL;
	//Create pacekt
	RTCPCompoundPacket* rtcp = new RTCPCompoundPacket();
	//Init pointers
	BYTE *buffer = data;
	DWORD bufferLen = size;
	//Parse
	while (bufferLen)
	{
		RTCPPacket *packet = NULL;
		//Get header
		rtcp_common_t* header = (rtcp_common_t*) buffer;
		//Get type
		RTCPPacket::Type type = (RTCPPacket::Type)header->pt;
		//Create new packet
		switch (type)
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
				Debug("Unknown rtcp packet type [%d]\n",header->pt);
		}
		//Get size of the packet
		DWORD len = GetRTCPHeaderLength(header);
		//Check len
		if (len>bufferLen)
		{
			//error
			Error("Wrong rtcp packet size\n");
			//Exit
			return NULL;
		}
		//parse
		if (packet && packet->Parse(buffer,len))
			//Add packet
			rtcp->AddRTCPacket(packet);
		//Remove size
		bufferLen -= len;
		//Increase pointer
		buffer    += len;
	}

	//Return it
	return rtcp;
}