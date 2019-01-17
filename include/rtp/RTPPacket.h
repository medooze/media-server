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
#include <memory>

class RTPPacket
{
public:
	const static DWORD MaxExtSeqNum = 0xFFFFFFFF;
public:
	using shared = std::shared_ptr<RTPPacket>;
	using unique = std::unique_ptr<RTPPacket>;

public:
	RTPPacket(MediaFrame::Type media,BYTE codec);
	RTPPacket(MediaFrame::Type media,BYTE codec,const RTPHeader &header, const RTPHeaderExtension &extension);
	virtual ~RTPPacket();

	RTPPacket::shared Clone();
	
	DWORD Serialize(BYTE* data,DWORD size,const RTPMap& extMap) const;
	
	bool	SetPayload(const BYTE *data,DWORD size);
	bool	SkipPayload(DWORD skip);
	bool	PrefixPayload(BYTE *data,DWORD size);
	
	bool	RecoverOSN();

	virtual void Dump();
	
	//Setters
	void SetTimestamp(DWORD timestamp)	{ header.timestamp = timestamp;		}
	void SetSeqNum(WORD seq)		{ header.sequenceNumber = seq;		}
	void SetExtSeqNum(DWORD extSeq)		{ header.sequenceNumber = (WORD)extSeq; this->cycles = extSeq >> 16;	}
	void SetMark(bool mark)			{ header.mark = mark;			}
	void SetSSRC(DWORD ssrc)		{ header.ssrc = ssrc;			}
	void SetPayloadType(DWORD payloadType)	{ header.payloadType = payloadType;	}
	void SetType(DWORD payloadType)		{ SetType(payloadType);			} //Deprecated
	void SetPadding(WORD padding)		{ header.padding = padding;		}
	void SetMediaTpe(MediaFrame::Type media){ this->media = media;			}
	void SetCodec(BYTE codec)		{ this->codec = codec;			}
	void SetSeqCycles(WORD cycles)		{ this->cycles = cycles;		}
	void SetClockRate(DWORD rate)		{ this->clockRate = rate;		}

	void SetMediaLength(DWORD len)		{ this->payloadLen = len;		}
	
	//Getters
	MediaFrame::Type GetMedia()	const { return media;				} //Deprecated
	MediaFrame::Type GetMediaType()	const { return media;				}
	BYTE  GetCodec()		const { return codec;				}
	
	
	
	BYTE* GetMediaData()		      { return payload;				}
	const BYTE* GetMediaData()	const { return payload;				}
	DWORD GetMediaLength()		const { return payloadLen;			}
	DWORD GetMaxMediaLength()	const { return SIZE;				}
	
	bool  GetMark()			const { return header.mark;			}
	DWORD GetTimestamp()		const { return header.timestamp;		}
	WORD  GetSeqNum()		const { return header.sequenceNumber;		}
	DWORD GetSSRC()			const { return header.ssrc;			}
	DWORD GetPayloadType()		const { return header.payloadType;		}
	WORD  GetPadding()		const { return header.padding;			}
	
	WORD  GetSeqCycles()		const { return cycles;				}
	DWORD GetClockRate()		const { return clockRate;			}
	DWORD GetExtSeqNum()		const { return ((DWORD)cycles)<<16 | GetSeqNum();			}
	DWORD GetClockTimestamp()	const { return static_cast<QWORD>(GetTimestamp())*1000/clockRate;	}

	//Extensions
	void  SetAbsSentTime(QWORD absSentTime)						{ header.extension = extension.hasAbsSentTime     = true; extension.absSentTime = absSentTime;	}
	void  SetTimeOffset(int timeOffset)						{ header.extension = extension.hasTimeOffset      = true; extension.timeOffset = timeOffset;	}
	void  SetTransportSeqNum(DWORD seq)						{ header.extension = extension.hasTransportWideCC = true; extension.transportSeqNum = seq;	}
	void  SetFrameMarkings(const RTPHeaderExtension::FrameMarks& frameMarks )	{ header.extension = extension.hasFrameMarking    = true; extension.frameMarks = frameMarks;	}
	void  SetRId(const std::string &rid)						{ header.extension = extension.hasRId		  = true; extension.rid = rid;			}
	void  SetRepairedId(const std::string &repairedId)				{ header.extension = extension.hasRepairedId	  = true; extension.repairedId = repairedId;	}
	void  SetMediaStreamId(const std::string &mid)					{ header.extension = extension.hasMediaStreamId   = true; extension.mid = mid;			}
	
	//Disable extensions
	void  DisableAbsSentTime()	{ extension.hasAbsSentTime     = false; CheckExtensionMark(); }
	void  DisableTimeOffset()	{ extension.hasTimeOffset      = false; CheckExtensionMark(); }
	void  DisableTransportSeqNum()	{ extension.hasTransportWideCC = false; CheckExtensionMark(); }
	void  DisableFrameMarkings()	{ extension.hasFrameMarking    = false; CheckExtensionMark(); }
	void  DisableRId()		{ extension.hasRId	       = false; CheckExtensionMark(); }
	void  DisableRepairedId()	{ extension.hasRepairedId      = false; CheckExtensionMark(); }
	void  DisableMediaStreamId()	{ extension.hasMediaStreamId   = false; CheckExtensionMark(); }
	

	QWORD GetAbsSendTime()			const	{ return extension.absSentTime;		}
	int   GetTimeOffset()			const	{ return extension.timeOffset;		}
	bool  GetVAD()				const	{ return extension.vad;			}
	BYTE  GetLevel()			const	{ return extension.level;		}
	WORD  GetTransportSeqNum()		const	{ return extension.transportSeqNum;	}
	const std::string& GetRId()		const	{ return extension.rid;			}
	const std::string& GetRepairedId()	const	{ return extension.repairedId;		}
	const std::string& GetMediaStreamId()	const	{ return extension.mid;	}
	const RTPHeaderExtension::FrameMarks& GetFrameMarks() const { return extension.frameMarks; }
	bool  HasAudioLevel()			const	{ return extension.hasAudioLevel;	}
	bool  HasAbsSentTime()			const	{ return extension.hasAbsSentTime;	}
	bool  HasTimeOffeset()			const   { return extension.hasTimeOffset;	}
	bool  HasTransportWideCC()		const   { return extension.hasTransportWideCC;	}
	bool  HasFrameMarkings()		const   { return extension.hasFrameMarking;	}
	bool  HasRId()				const   { return extension.hasRId;		}
	bool  HasRepairedId()			const   { return extension.hasRepairedId;	}
	bool  HasMediaStreamId()		const   { return extension.hasMediaStreamId;	}
	
	
	QWORD GetTime()	const		{ return time;		}
	void  SetTime(QWORD time )	{ this->time = time;	}
	
	const RTPHeader&		GetRTPHeader()		const { return header;		}
	const RTPHeaderExtension&	GetRTPHeaderExtension()	const { return extension;	}
	
protected:
	void  CheckExtensionMark()	{ header.extension =  extension.hasAudioLevel
						|| extension.hasAbsSentTime 
						|| extension.hasTimeOffset
						|| extension.hasTransportWideCC
						|| extension.hasFrameMarking
						|| extension.hasRId
						|| extension.hasRepairedId
						|| extension.hasMediaStreamId; }

private:
	static const DWORD SIZE = 1700;
	static const DWORD PREFIX = 200;
private:
	MediaFrame::Type media;
	BYTE		codec;
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

