/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPFullIntraRequest.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:01
 */

#ifndef RTCPFULLINTRAREQUEST_H
#define RTCPFULLINTRAREQUEST_H
#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"

class RTCPFullIntraRequest : public RTCPPacket
{
public:
	RTCPFullIntraRequest();
	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	DWORD ssrc = 0;
};
#endif /* RTCPFULLINTRAREQUEST_H */

