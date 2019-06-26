/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPBye.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:00
 */

#ifndef RTCPBYE_H
#define RTCPBYE_H
#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"
#include <vector>

class RTCPBye : public RTCPPacket
{
public:
	using shared = std::shared_ptr<RTCPBye>;
public:
	RTCPBye();
	RTCPBye(const std::vector<DWORD> &ssrcs,const char* reason);
	~RTCPBye();
	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
	virtual void Dump();
	static RTCPBye::shared Create(const std::vector<DWORD> &ssrcs,const char* reason)
	{
		return std::make_shared<RTCPBye>(ssrcs,reason);
	}
	const std::vector<DWORD>& GetSSRCs() const  { return ssrcs; };
private:
	std::vector<DWORD> ssrcs;
	char* reason = NULL;
};
#endif /* RTCPBYE_H */

