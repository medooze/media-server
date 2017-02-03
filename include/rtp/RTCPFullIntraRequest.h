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

class RTCPFullIntraRequest : public RTCPPacket
{
public:
	RTCPFullIntraRequest();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	DWORD ssrc;
};
#endif /* RTCPFULLINTRAREQUEST_H */

