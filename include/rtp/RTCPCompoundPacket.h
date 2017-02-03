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

class RTCPCompoundPacket
{
public:
	static bool IsRTCP(BYTE *data,DWORD size)
	{
		//Check size
		if (size<sizeof(rtcp_common_t))
			//No
			return 0;
		//Get RTCP common header
		rtcp_common_t* header = (rtcp_common_t*)data;
		//Check version
		if (header->version!=2)
			//No
			return 0;
		//Check type
		if (header->pt<200 ||  header->pt>206)
			//It is no
			return 0;
		//RTCP
		return 1;
	}

	~RTCPCompoundPacket()
	{
		//Fir each oen
		for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
			//delete
			delete((*it));
	}

	DWORD GetSize() const
	{
		DWORD size = 0;
		//Calculate
		for(RTCPPackets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
			//Append size
			size = sizeof(rtcp_common_t)+(*it)->GetSize();
		//Return total size
		return size;
	}

	DWORD Serialize(BYTE *data,DWORD size)
	{
		DWORD len = 0;
		//Check size
		if (size<GetSize())
			//Error
			return 0;
		//For each one
		for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
			//Serialize
			len +=(*it)->Serialize(data+len,size-len);
		//Exit
		return len;
	}
	void Dump();

	void	    AddRTCPacket(RTCPPacket* packet)	{ packets.push_back(packet);	}
	DWORD	    GetPacketCount()	const		{ return packets.size();	}
	const RTCPPacket* GetPacket(DWORD num) const		{ return packets[num];		}

	static RTCPCompoundPacket* Parse(BYTE *data,DWORD size);
private:
	typedef std::vector<RTCPPacket*> RTCPPackets;
private:
	RTCPPackets packets;
};

#endif /* RTCPCOMPOUNDPACKET_H */

