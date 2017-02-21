/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPHeaderExtension.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:51
 */

#ifndef RTPHEADEREXTENSION_H
#define RTPHEADEREXTENSION_H
#include "config.h"
#include "tools.h"
#include "rtp/RTPMap.h"

class RTPHeaderExtension
{
public:
	enum Type {
		UNKNOWN			= 0,
		SSRCAudioLevel		= 1,
		TimeOffset		= 2,
		AbsoluteSendTime	= 3,
		CoordinationOfVideoOrientation	= 4,
		TransportWideCC		= 5
	};
	
	static Type GetExtensionForName(const char* ext)
	{
		if	(strcasecmp(ext,"urn:ietf:params:rtp-hdrext:toffset")==0)						return TimeOffset;
		else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")==0)			return AbsoluteSendTime;
		else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:ssrc-audio-level")==0)					return SSRCAudioLevel;
		else if (strcasecmp(ext,"urn:3gpp:video-orientation")==0)							return CoordinationOfVideoOrientation;
		else if (strcasecmp(ext,"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01")==0)	return TransportWideCC;
		return UNKNOWN;
	}

	static const char* GetNameFor(Type ext)
	{
		switch (ext)
		{
			case TimeOffset:			return "TimeOffset";
			case AbsoluteSendTime:			return "AbsoluteSendTime";
			case SSRCAudioLevel:			return "SSRCAudioLevel";
			case CoordinationOfVideoOrientation:	return "CoordinationOfVideoOrientation";
			case TransportWideCC:			return "TransportWideCC";
			default:				return "unknown";
		}
	}
	
	struct VideoOrientation
	{
		bool facing;
		bool flip;
		BYTE rotation;
		
		VideoOrientation()
		{
			facing		= 0;
			flip		= 0;
			rotation	= 0;
		}
	};
	
public:
	RTPHeaderExtension()
	{
		absSentTime = 0;
		timeOffset = 0;
		vad = 0;
		level = 0;
		transportSeqNum = 0;
		hasAbsSentTime = 0;
		hasTimeOffset =  0;
		hasAudioLevel = 0;
		hasVideoOrientation = 0;
		hasTransportWideCC = 0;
	}
	
	DWORD Parse(const RTPMap &extMap,const BYTE* data,const DWORD size);
	DWORD Serialize(const RTPMap &extMap,BYTE* data,const DWORD size) const;
	void  Dump() const;
public:
	QWORD	absSentTime;
	int	timeOffset;
	bool	vad;
	BYTE	level;
	WORD	transportSeqNum;
	VideoOrientation cvo;
	bool    hasAbsSentTime;
	bool	hasTimeOffset;
	bool	hasAudioLevel;
	bool	hasVideoOrientation;
	bool	hasTransportWideCC;
};

#endif /* RTPHEADEREXTENSION_H */

