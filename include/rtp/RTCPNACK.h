/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPNACK.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:02
 */

#ifndef RTCPNACK_H
#define RTCPNACK_H
#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"

class RTCPNACK : public RTCPPacket
{
public:
	RTCPNACK();
	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	DWORD ssrc = 0;
	WORD fsn = 0;
	WORD blp = 0;
};

#endif /* RTCPNACK_H */

