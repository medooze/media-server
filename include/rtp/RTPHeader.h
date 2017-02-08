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
	RTPHeader();

	DWORD Parse(const BYTE* data,const DWORD size);
	DWORD Serialize(BYTE* data,const DWORD size) const;
	DWORD GetSize() const;
	void Dump() const;
public:	
	BYTE    version;
	bool	padding;
	bool	extension;
	bool	mark;
	BYTE	payloadType;
	DWORD	sequenceNumber;
	DWORD	timestamp;
	DWORD	ssrc;

	std::list<DWORD> csrcs;
};

#endif /* RTPHEADER_H */

