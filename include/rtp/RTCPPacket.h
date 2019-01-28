/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPPacket.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:58
 */

#ifndef RTCPPACKET_H
#define RTCPPACKET_H
#include "config.h"
#include "rtp/RTCPCommonHeader.h"
#include "rtp/RTCPReport.h"
#include <vector>
#include <memory>

class RTCPPacket
{
public:
	using shared = std::shared_ptr<RTCPPacket>;
	
	enum Type{
		FullIntraRequest	= 192,
		NACK			= 193,
		ExtendedJitterReport	= 195,
		SenderReport		= 200,
		ReceiverReport		= 201,
		SDES			= 202,
		Bye			= 203,
		App			= 204,
		RTPFeedback		= 205,
		PayloadFeedback		= 206
	};

	static const char* TypeToString(Type type)
	{
		switch (type)
		{
			case RTCPPacket::SenderReport:
				return "SenderReport";
			case RTCPPacket::ReceiverReport:
				return "ReceiverReport";
			case RTCPPacket::SDES:
				return "SDES";
			case RTCPPacket::Bye:
				return "Bye";
			case RTCPPacket::App:
				return "App";
			case RTCPPacket::RTPFeedback:
				return "RTPFeedback";
			case RTCPPacket::PayloadFeedback:
				return "PayloadFeedback";
			case RTCPPacket::FullIntraRequest:
				return "FullIntraRequest";
			case RTCPPacket::NACK:
				return "NACK";
			case RTCPPacket::ExtendedJitterReport:
				return "ExtendedJitterReport";
		}
		return("Unknown");
	}
protected:
	RTCPPacket(Type type)
	{
		this->type = type;
	}
public:
	// Must have virtual destructor to ensure child class's destructor is called
	virtual ~RTCPPacket(){};
	Type GetType() const {return type; }
	virtual void Dump();
	virtual DWORD GetSize() = 0;
	virtual DWORD Parse(const BYTE* data,DWORD size) = 0;
	virtual DWORD Serialize(BYTE* data,DWORD size) = 0;
protected:
	typedef std::vector<RTCPReport::shared> Reports;
private:
	Type type;
};


#endif /* RTCPPACKET_H */

