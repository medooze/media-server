/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPCommonHeader.h
 * Author: Sergio
 *
 * Created on 8 de febrero de 2017, 10:05
 */

#ifndef RTCPCOMMONHEADER_H
#define RTCPCOMMONHEADER_H
#include "config.h"
#include "tools.h"



class RTCPCommonHeader
{
public:
	RTCPCommonHeader() = default;

	DWORD Parse(const BYTE* data,const DWORD size);
	DWORD Serialize(BYTE* data,const DWORD size) const;
	void Dump() const;
	
public:
	static DWORD GetSize() { return 4; }
	
public:	
	BYTE count	= 0; /* varies by packet type :5*/
	bool padding	= 0; /* padding flag */
	BYTE version	= 2; /* protocol version :2 */
	BYTE packetType = 0; /* RTCP packet type */
	WORD length	= 0; /* pkt len in words, w/o this word */
};

#endif /* RTCPCOMMONHEADER_H */

