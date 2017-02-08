/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPPacket.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:52
 */

#ifndef RTPPACKET_H
#define RTPPACKET_H
#include "config.h"
#include "media.h"
#include "rtp/RTPHeader.h"
#include "rtp/RTPHeaderExtension.h"

class RTPPacket
{
public:
	const static DWORD MaxExtSeqNum = 0xFFFFFFFF;
public:
	

public:
	RTPPacket(MediaFrame::Type media,DWORD codec);
	RTPPacket(MediaFrame::Type media,DWORD codec,const RTPHeader &header, const RTPHeaderExtension extension);
	virtual ~RTPPacket();

	RTPPacket* Clone();
	
	bool	SetPayload(BYTE *data,DWORD size);
	bool	SkipPayload(DWORD skip);
	bool	PrefixPayload(BYTE *data,DWORD size);
	virtual void Dump();
	
	//Setters
	void SetTimestamp(DWORD timestamp)	{ header.timestamp = timestamp;		}
	void SetSeqNum(WORD seq)		{ header.sequenceNumber = seq;		}
	void SetMark(bool mark)			{ header.mark = mark;			}
	void SetSSRC(DWORD ssrc)		{ header.ssrc = ssrc;			}
	void SetType(DWORD payloadType)		{ header.payloadType = payloadType;	}
	
	void SetCodec(DWORD codec)		{ this->codec = codec;			}
	void SetSeqCycles(WORD cycles)		{ this->cycles = cycles;		}
	void SetClockRate(DWORD rate)		{ this->clockRate = rate;		}

	void SetMediaLength(DWORD len)		{ this->payloadLen = len;		}
	
	//Getters
	MediaFrame::Type GetMedia()	const { return media;				}
	DWORD GetCodec()		const { return codec;				}
	
	
	
	BYTE* GetMediaData()		      { return payload;				}
	DWORD GetMediaLength()		const { return payloadLen;			}
	DWORD GetMaxMediaLength()	const { return SIZE;				}
	
	bool  GetMark()			const { return header.mark;			}
	DWORD GetTimestamp()		const { return header.timestamp;		}
	WORD  GetSeqNum()		const { return header.sequenceNumber;		}
	DWORD GetSSRC()			const { return header.ssrc;			}
	DWORD GetPayloadType()		const { return header.payloadType;		}
	
	WORD  GetSeqCycles()		const { return cycles;				}
	DWORD GetClockRate()		const { return clockRate;			}
	DWORD GetExtSeqNum()		const { return ((DWORD)cycles)<<16 | GetSeqNum();			}
	DWORD GetClockTimestamp()	const { return static_cast<QWORD>(GetTimestamp())*1000/clockRate;	}

	//Extensions
	void  SetAbsSentTime(QWORD absSentTime)	{ extension.absSentTime = absSentTime;	}
	void  SetTimeOffset(int timeOffset)     { extension.timeOffset = timeOffset;	}
	
	QWORD GetAbsSendTime()		const	{ return extension.absSentTime;		}
	int   GetTimeOffset()		const	{ return extension.timeOffset;		}
	bool  GetVAD()			const	{ return extension.vad;			}
	BYTE  GetLevel()		const	{ return extension.level;		}
	WORD  GetTransportSeqNum()	const	{ return extension.transportSeqNum;	}
	bool  HasAudioLevel()		const	{ return extension.hasAudioLevel;	}
	bool  HasAbsSentTime()		const	{ return extension.hasAbsSentTime;	}
	bool  HasTimeOffeset()		const   { return extension.hasTimeOffset;	}
	bool  HasTransportWideCC()	const   { return extension.hasTransportWideCC;	}
	
	QWORD GetTime()	const		{ return time;		}
	void  SetTime(QWORD time )	{ this->time = time;	}
	
	const RTPHeader&		GetRTPHeader()		const { return header;		}
	const RTPHeaderExtension&	GetRTPHeaderExtension()	const { return extension;	}

private:
	static const DWORD SIZE = 1700;
	static const DWORD PREFIX = 200;
private:
	MediaFrame::Type media;
	DWORD		codec;
	DWORD		clockRate;
	WORD		cycles;
	
	RTPHeader	   header;
	RTPHeaderExtension extension;
	
	BYTE	buffer[SIZE+PREFIX];
	BYTE*   payload;
	DWORD	payloadLen;

	QWORD time;
};
#endif /* RTPPACKET_H */

