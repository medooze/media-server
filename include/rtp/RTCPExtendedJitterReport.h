/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPExtendedJitterReport.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:01
 */

#ifndef RTCPEXTENDEDJITTERREPORT_H
#define RTCPEXTENDEDJITTERREPORT_H
#include "config.h"
#include "rtp/RTCPPacket.h"
#include <vector>

class RTCPExtendedJitterReport : public RTCPPacket
{
public:
	RTCPExtendedJitterReport();

	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	typedef std::vector<DWORD> Jitters;
private:
	Jitters jitters;
};
#endif /* RTCPEXTENDEDJITTERREPORT_H */

