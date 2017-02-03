/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPTimedPacket.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:54
 */

#ifndef RTPTIMEDPACKET_H
#define RTPTIMEDPACKET_H


class RTPTimedPacket:
	public RTPPacket
{
public:
	RTPTimedPacket(MediaFrame::Type media,DWORD codec) : RTPPacket(media,codec,codec)
	{
		//Set time
		time = ::getTime()/1000;
	}

	RTPTimedPacket(MediaFrame::Type media,BYTE *data,DWORD size) : RTPPacket(media,data,size)
	{
		//Set time
		time = ::getTime()/1000;
	}

	RTPTimedPacket(MediaFrame::Type media,DWORD codec,DWORD type) : RTPPacket(media,codec,type)
	{
		//Set time
		time = ::getTime();
	}

	RTPTimedPacket* Clone()
	{
		//New one
		RTPTimedPacket* cloned = new RTPTimedPacket(GetMedia(),GetCodec(),GetType());
		//Set attrributes
		cloned->SetClockRate(GetClockRate());
		cloned->SetMark(GetMark());
		cloned->SetSeqNum(GetSeqNum());
		cloned->SetSeqCycles(GetSeqCycles());
		cloned->SetTimestamp(GetTimestamp());
		//Set payload
		cloned->SetPayload(GetMediaData(),GetMediaLength());
		//Set time
		cloned->SetTime(GetTime());
		//Check if we have extension
		if (HasAbsSentTime())
			//Set abs time
			cloned->SetAbsSentTime(GetAbsSendTime());
		//Check if we have extension
		if (HasTimeOffeset())
			//Set abs time
			cloned->SetAbsSentTime(GetTimeOffset());
		//Return it
		return cloned;
	}

	QWORD GetTime()	const		{ return time;		}
	void  SetTime(QWORD time )	{ this->time = time;	}
private:
	QWORD time;
};

#endif /* RTPTIMEDPACKET_H */

