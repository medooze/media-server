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
#include <map>

class RTPMap :
	public std::map<BYTE,BYTE>
{
public:
	BYTE GetCodecForType(BYTE type) const
	{
		//Find the type in the map
		const_iterator it = find(type);
		//Check it is in there
		if (it==end())
			//Exit
			return NotFound;
		//It is our codec
		return it->second;
	}
	
	BYTE GetTypeForCodec(BYTE codec) const
	{
		//Try to find it in the map
		for (const_iterator it = begin(); it!=end(); ++it)
			//Is it ourr codec
			if (it->second==codec)
				//return it
				return it->first;
		//Exit
		return NotFound;
	}
public:
	static const BYTE NotFound = -1;
};


#endif /* RTPMAP_H */

