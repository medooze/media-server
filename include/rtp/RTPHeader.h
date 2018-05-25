/* 
 * File:   RTPHeader.h
 * Author: Sergio
 *
 * Created on 7 de febrero de 2017, 11:00
 */

#ifndef RTPHEADER_H
#define RTPHEADER_H
#include "config.h"
#include <list>

class RTPHeader
{
public:
	RTPHeader() = default;

	DWORD Parse(const BYTE* data,const DWORD size);
	DWORD Serialize(BYTE* data,const DWORD size) const;
	DWORD GetSize() const;
	void Dump() const;
public:	
	BYTE    version		= 2;
	bool	padding		= false;
	bool	extension	= false;
	bool	mark		= false;
	BYTE	payloadType	= 0;
	WORD	sequenceNumber	= 0;
	DWORD	timestamp	= 0;
	DWORD	ssrc		= 0;

	std::list<DWORD> csrcs;
};

#endif /* RTPHEADER_H */
