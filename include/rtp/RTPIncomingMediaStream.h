/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPIncomingMediaStream.h
 * Author: Sergio
 *
 * Created on 21 de febrero de 2019, 16:06
 */

#ifndef RTPINCOMINGMEDIASTREAM_H
#define RTPINCOMINGMEDIASTREAM_H


class RTPIncomingMediaStream
{
public:
	class Listener 
	{
	public:
		virtual void onRTP(RTPIncomingMediaStream* stream,const RTPPacket::shared& packet) = 0;
		virtual void onEnded(RTPIncomingMediaStream* stream) = 0;
	};
	
	virtual void AddListener(Listener* listener) = 0;
	virtual void RemoveListener(Listener* listener) = 0;
	virtual DWORD GetMediaSSRC() = 0;
};

#endif /* RTPINCOMINGMEDIASTREAM_H */

