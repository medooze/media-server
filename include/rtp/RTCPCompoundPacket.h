/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPCompoundPacket.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:04
 */

#ifndef RTCPCOMPOUNDPACKET_H
#define RTCPCOMPOUNDPACKET_H
#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"
#include <vector>

class RTCPCompoundPacket
{
public:
	static bool IsRTCP(BYTE *data,DWORD size)
	{
		//Check size
		if (size<4)
			//No
			return 0;
		//Check version
		if ((data[0]>>6)!=2)
			//No
			return 0;
		//Check type
		if (data[1]<200 ||  data[1]>206)
			//It is no
			return 0;
		//RTCP
		return 1;
	}

	~RTCPCompoundPacket();
	DWORD GetSize() const;
	DWORD Serialize(BYTE *data,DWORD size) const;
	void Dump() const;

	void	    AddRTCPacket(RTCPPacket* packet)		{ packets.push_back(packet);	}
	DWORD	    GetPacketCount()			const	{ return packets.size();	}
	const RTCPPacket* GetPacket(DWORD num)		const	{ return packets[num];		}

	static RTCPCompoundPacket* Parse(BYTE *data,DWORD size);
private:
	typedef std::vector<RTCPPacket*> RTCPPackets;
private:
	RTCPPackets packets;
};

#endif /* RTCPCOMPOUNDPACKET_H */

