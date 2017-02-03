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
#include <vector>

class RTCPBye : public RTCPPacket
{
public:
	RTCPBye();
	~RTCPBye();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	typedef std::vector<DWORD> SSRCs;
private:
	SSRCs ssrcs;
	char* reason;
};
#endif /* RTCPBYE_H */

