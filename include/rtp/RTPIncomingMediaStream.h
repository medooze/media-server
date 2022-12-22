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

#include "config.h"
#include "rtp/RTPPacket.h"
#include "TimeService.h"

#include <vector>

class RTPIncomingMediaStream
{
public:
	using shared = std::shared_ptr<RTPIncomingMediaStream>;
public:
	class Listener 
	{
	public:
		virtual ~Listener() = default;
		virtual void onRTP(const RTPIncomingMediaStream* stream,const RTPPacket::shared& packet) = 0;
		virtual void onRTP(const RTPIncomingMediaStream* stream,const std::vector<RTPPacket::shared>& packets)
		{
			for (const auto& packet : packets)
				onRTP(stream,packet);
		};
		virtual void onBye(const RTPIncomingMediaStream* stream) = 0;
		virtual void onEnded(const RTPIncomingMediaStream* stream) = 0;
	};
	virtual ~RTPIncomingMediaStream() = default;
	virtual void AddListener(Listener* listener) = 0;
	virtual void RemoveListener(Listener* listener) = 0;
	virtual DWORD GetMediaSSRC() const = 0;
	virtual TimeService& GetTimeService() = 0;
	virtual void Mute(bool muting) = 0;
};

#endif /* RTPINCOMINGMEDIASTREAM_H */

