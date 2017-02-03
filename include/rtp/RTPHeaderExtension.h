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

class RTPPacket;
class RTPHeaderExtension
{
friend class  RTPPacket;
public:
	
	
	enum Type {
		SSRCAudioLevel		= 1,
		TimeOffset		= 2,
		AbsoluteSendTime	= 3,
		CoordinationOfVideoOrientation	= 4,
		TransportWideCC		= 5
	};
	
	struct VideoOrientation
	{
		BYTE reserved: 4;
		BYTE facing: 1;
		BYTE flip: 1;
		BYTE rotation: 2;
	};
	
public:
	RTPHeaderExtension()
	{
		absSentTime = 0;
		timeOffset = 0;
		vad = 0;
		level = 0;
		transportSeqNum = 0;
		*((BYTE*)&cvo) = 0;
		hasAbsSentTime = 0;
		hasTimeOffset =  0;
		hasAudioLevel = 0;
		hasVideoOrientation = 0;
		hasTransportWideCC = 0;
	}
protected:
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

