/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPApp.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:03
 */

#ifndef RTCPAPP_H
#define RTCPAPP_H

class RTCPApp : public RTCPPacket
{
public:
	RTCPApp();
	virtual ~RTCPApp();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	BYTE subtype;
	DWORD ssrc;
	char name[4];
	BYTE *data;
	DWORD size;
};	
	
#endif /* RTCPAPP_H */

