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
#include "config.h"
#include "rtp/RTPPacket.h"

class RTPPacketSched :
	public RTPPacket
{
public:
	using shared = std::shared_ptr<RTPPacketSched>; 
public:
	RTPPacketSched(MediaFrame::Type media,DWORD codec) : RTPPacket(media,codec)
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

