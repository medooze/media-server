/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPPacketSched.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:53
 */

#ifndef RTPPACKETSCHED_H
#define RTPPACKETSCHED_H


class RTPPacketSched :
	public RTPPacket
{
public:
	RTPPacketSched(MediaFrame::Type media,DWORD codec) : RTPPacket(media,codec,codec)
	{
		//Set sending type
		sendingTime = 0;
	}

	RTPPacketSched(MediaFrame::Type media,BYTE *data,DWORD size) : RTPPacket(media,data,size)
	{
		//Set sending type
		sendingTime = 0;
	}

	RTPPacketSched(MediaFrame::Type media,DWORD codec,DWORD type) : RTPPacket(media,codec,type)
	{
		//Set sending type
		sendingTime = 0;
	}

	void	SetSendingTime(DWORD sendingTime)	{ this->sendingTime = sendingTime;	}
	DWORD	GetSendingTime()			{ return sendingTime;			}
private:
	DWORD sendingTime;
};


#endif /* RTPPACKETSCHED_H */

