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
#include <memory>

class RTCPCompoundPacket
{
public:
	using shared = std::shared_ptr<RTCPCompoundPacket>;
	
public:
	static bool IsRTCP(const BYTE *data,DWORD size)
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
	static RTCPCompoundPacket::shared Parse(const BYTE *data,DWORD size);
	static RTCPCompoundPacket::shared Create()
	{
		return  std::make_shared<RTCPCompoundPacket>();
	}
	
	template <typename Type>
	static RTCPCompoundPacket::shared Create(const Type& packet)
	{
		return  std::make_shared<RTCPCompoundPacket>(std::static_pointer_cast<RTCPPacket>(packet));
	}
public:
	RTCPCompoundPacket() = default;
	RTCPCompoundPacket(const RTCPPacket::shared& packet);
	~RTCPCompoundPacket() = default;
	DWORD GetSize() const;
	DWORD Serialize(BYTE *data,DWORD size) const;
	void Dump() const;

	
	void			 AddPacket(const RTCPPacket::shared& packet)	{ packets.push_back(packet);	}
	DWORD			 GetPacketCount()			const	{ return packets.size();	}
	const RTCPPacket::shared GetPacket(DWORD num)			const	{ return packets[num];		}
	template<typename Type>
	const std::shared_ptr<Type> GetPacket(DWORD num)		const	{ return std::static_pointer_cast<Type>(packets[num]); }
	
	template<typename Type,class ...Args>
	std::shared_ptr<Type> CreatePacket(Args... args)
	{
		auto packet =  std::make_shared<Type>(std::forward<Args>(args)... );
		AddPacket(std::static_pointer_cast<RTCPPacket>(packet));
		return packet;
	}

private:
	typedef std::vector<RTCPPacket::shared> RTCPPackets;
	
private:
	RTCPPackets packets;
};

#endif /* RTCPCOMPOUNDPACKET_H */

