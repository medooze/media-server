/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPMap.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:50
 */

#ifndef RTPMAP_H
#define RTPMAP_H
#include "config.h"
#include "log.h"
#include "media.h"
#include <map>

class RTPMap
{
private:
	std::map<BYTE, BYTE> data;
	std::array<BYTE, 256> forward, reverse;
public:
	RTPMap() { clear(); }

	bool empty() const
	{
		return data.empty();
	}

	void clear()
	{
		data.clear();
		forward.fill(NotFound);
		reverse.fill(NotFound);
	}

	BYTE GetFirstCodecType()
	{
		return data.cbegin()->first;
	}

	void SetCodecForType(BYTE type, BYTE codec)
	{
		data[type] = codec;
		forward[type] = codec;
		reverse[codec] = type;
	}

	BYTE GetCodecForType(BYTE type) const { return forward[type]; }
	BYTE GetTypeForCodec(BYTE codec) const { return reverse[codec]; }
	bool HasCodec(BYTE codec) const { return reverse[codec] != NotFound; }

	void Dump(MediaFrame::Type media) const;
public:
	static const BYTE NotFound = -1;
};


#endif /* RTPMAP_H */

