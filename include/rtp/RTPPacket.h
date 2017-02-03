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


	
class RTPPacket
{
public:
	const static DWORD MaxExtSeqNum = 0xFFFFFFFF;
public:
	

public:
	RTPPacket(MediaFrame::Type media,DWORD codec)
	{
		this->media = media;
		//Set coced
		SetCodec(codec);
		//Get header pointer
		header = (rtp_hdr_t*) buffer;
		//empty header
		memset(header,0,sizeof(rtp_hdr_t));
		//set version and type
		SetVersion(2);
		SetType(codec);
		//NO seq cycles
		cycles = 0;
		//Default clock rates
		switch(media)
		{
			case MediaFrame::Video:
				clockRate = 90000;
				break;
			case MediaFrame::Audio:
				clockRate = 8000;
				break;
			default:
				clockRate = 1000;
		}
	}

	RTPPacket(MediaFrame::Type media,BYTE *data,DWORD size)
	{
		this->media = media;
		//Get header pointer
		header = (rtp_hdr_t*) buffer;
		//Set Data
		SetData(data,size);
		//Set codec as type for now
		codec = GetType();
		//NO seq cycles
		cycles = 0;
		//Default clock rates
		switch(media)
		{
			case MediaFrame::Video:
				clockRate = 90000;
				break;
			case MediaFrame::Audio:
				clockRate = 8000;
				break;
			default:
				clockRate = 1000;
		}
	}

	RTPPacket(MediaFrame::Type media,DWORD codec,DWORD type)
	{
		this->media = media;
		this->codec = codec;
		//Get header pointer
		header = (rtp_hdr_t*) buffer;
		//empty header
		memset(header,0,sizeof(rtp_hdr_t));
		//set version and type
		SetVersion(2);
		SetType(type);
		//NO seq cycles
		cycles = 0;
		//Default clock rates
		switch(media)
		{
			case MediaFrame::Video:
				clockRate = 90000;
				break;
			case MediaFrame::Audio:
				clockRate = 8000;
				break;
			default:
				clockRate = 1000;
		}
	}
	
	virtual ~RTPPacket(){}

	RTPPacket* Clone()
	{
		//New one
		RTPPacket* cloned = new RTPPacket(GetMedia(),GetCodec(),GetType());
		//Set attrributes
		cloned->SetClockRate(GetClockRate());
		cloned->SetMark(GetMark());
		cloned->SetSeqNum(GetSeqNum());
		cloned->SetSeqCycles(GetSeqCycles());
		cloned->SetTimestamp(GetTimestamp());
		//Set payload
		cloned->SetPayload(GetMediaData(),GetMediaLength());
		//Return it
		return cloned;
	}

	//Setters
	void SetMediaLength(DWORD len)	{ this->len = len;			}
	void SetTimestamp(DWORD ts)	{ header->ts = htonl(ts);		}
	void SetSeqNum(WORD sn)		{ header->seq = htons(sn);		}
	void SetVersion(BYTE version)	{ header->version = version;		}
	void SetP(bool p)		{ header->p = p;			}
	void SetX(bool x)		{ header->x = x;			}
	void SetCC(BYTE cc)		{ header->cc = cc;			}
	void SetMark(bool mark)		{ header->m = mark;			}
	void SetSSRC(DWORD ssrc)	{ header->ssrc = htonl(ssrc);		}
	void SetCodec(DWORD codec)	{ this->codec = codec;			}
	void SetType(DWORD type)	{ header->pt = type;			}
	void SetSize(DWORD size)	{ len = size-GetRTPHeaderLen();		}
	void SetSeqCycles(WORD cycles)	{ this->cycles = cycles;		}
	void SetClockRate(DWORD rate)	{ this->clockRate = rate;		}
	

	//Getters
	MediaFrame::Type GetMedia()	const { return media;			}
	rtp_hdr_t*	GetRTPHeader()		const { return (rtp_hdr_t*)buffer;	}
	rtp_hdr_ext_t*	GeExtensionHeader()	const { return  GetX() ? (rtp_hdr_ext_t*)(buffer+sizeof(rtp_hdr_t)+4*header->cc) : NULL;	}
	DWORD GetRTPHeaderLen()		const { return sizeof(rtp_hdr_t)+4*header->cc+GetExtensionSize();	}
	WORD  GetExtensionType()	const { return ntohs(GeExtensionHeader()->ext_type);				}
	WORD  GetExtensionLength()	const { return GetX() ? ntohs(GeExtensionHeader()->len)*4 : 0;		}
	DWORD GetExtensionSize()	const { return GetX() ? GetExtensionLength()+sizeof(rtp_hdr_ext_t) : 0; };
	DWORD GetCodec()		const { return codec;				}
	BYTE  GetVersion()		const { return header->version;			}
	bool  GetX()			const { return header->x;			}
	bool  GetP()			const { return header->p;			}
	BYTE  GetCC()			const { return header->cc;			}
	DWORD GetType()			const { return header->pt;			}
	DWORD GetSize()			const { return len+GetRTPHeaderLen();		}
	BYTE* GetData()			      { return buffer;				}
	DWORD GetMaxSize()		const { return SIZE;				}
	BYTE* GetMediaData()		      { return buffer+GetRTPHeaderLen();	}
	DWORD GetMediaLength()		const { return len;				}
	DWORD GetMaxMediaLength()	const { return SIZE>GetRTPHeaderLen() ? SIZE-GetRTPHeaderLen() : 0;		}
	bool  GetMark()			const { return header->m;			}
	DWORD GetTimestamp()		const { return ntohl(header->ts);		}
	WORD  GetSeqNum()		const { return ntohs(header->seq);		}
	DWORD GetSSRC()			const { return ntohl(header->ssrc);		}
	WORD  GetSeqCycles()		const { return cycles;				}
	DWORD GetClockRate()		const { return clockRate;			}
	DWORD GetExtSeqNum()		const { return ((DWORD)cycles)<<16 | GetSeqNum();			}
	DWORD GetClockTimestamp()	const { return static_cast<QWORD>(GetTimestamp())*1000/clockRate;	}

	void  ProcessExtensions(const RTPMap &extMap);

	//Extensions
	void  SetAbsSentTime(QWORD absSentTime)	{ extension.absSentTime = absSentTime;	}
	void  SetTimeOffset(int timeOffset)     { extension.timeOffset = timeOffset;	}
	QWORD GetAbsSendTime()		const	{ return extension.absSentTime;		}
	int   GetTimeOffset()		const	{ return extension.timeOffset;		}
	bool  GetVAD()			const	{ return extension.vad;			}
	BYTE  GetLevel()		const	{ return extension.level;		}
	WORD  GetTransportSeqNum()		const	{ return extension.transportSeqNum;	}
	bool  HasAudioLevel()		const	{ return extension.hasAudioLevel;	}
	bool  HasAbsSentTime()		const	{ return extension.hasAbsSentTime;	}
	bool  HasTimeOffeset()		const   { return extension.hasTimeOffset;	}
	bool  HasTransportWideCC()	const   { return extension.hasTransportWideCC;	}

	DWORD SetExtensionHeader(BYTE* data,DWORD size)
	{
		//Get the length of the header extesion
		int len = sizeof(rtp_hdr_ext_t);
		//Check there is at least minimum size
		if (size<len)
			//Error
			return 0;
		//Get the header
		rtp_hdr_ext_t* headers = GeExtensionHeader();
		//Set it
		memcpy((BYTE*)headers,data,len);
		//The amount of consumed data	
		return len;
	}
	
	DWORD SetExtensionData(BYTE* data,DWORD size)
	{
		//Get extension data
		BYTE* ext = GetExtensionData();
		//Get extesnion lenght
		WORD length = GetExtensionLength();
		//Check sizes
		if (size<length)
			//Error
			return 0;
		//Copy it
		memcpy(ext,data,length);
		//Consumed data
		return length;
	}

	bool SetPayloadWithExtensionData(BYTE* data,DWORD size)
	{
		BYTE *d = data;
		DWORD s = size;
		
		//If extensions are enabled
		if (GetX()) 
		{
			//Set the extension header 
			int len = SetExtensionHeader(d,s);
			//Ensure we have copied something
			if (!len)
				return false;
			//Move pointer
			d+=len;
			s-=len;
			//Set the extension header 
			len = SetExtensionData(d,s);
			//Ensure we have copied something
			if (!len)
				return false;
			//Move pointer
			d+=len;
			s-=len;
		}
		//Now set payload
		return SetPayload(d,s);
		
	}
	
	bool SetPayload(BYTE *data,DWORD size)
	{
		//Check size
		if (size>GetMaxMediaLength())
			//Error
			return false;
		//Copy
		memcpy(GetMediaData(),data,size);
		//Ser size
		SetMediaLength(size);
		//good
		return true;
	}

	bool SetData(BYTE* data,DWORD size)
	{
		//Check
		if (size>SIZE)
			//Error
			return false;
		//Copy data
		memcpy(buffer,data,size);
		//Set size
		SetSize(size);
		//OK
		return true;
	}

	virtual void Dump()
	{
		Log("[RTPPacket %s codec=%d size=%d payload=%d]\n",MediaFrame::TypeToString(GetMedia()),GetCodec(),GetSize(),GetMediaLength());
		Log("\t[Header v=%d p=%d x=%d cc=%d m=%d pt=%d seq=%d ts=%d ssrc=%u len=%d]\n",GetVersion(),GetP(),GetX(),GetCC(),GetMark(),GetType(),GetSeqNum(),GetClockTimestamp(),GetSSRC(),GetRTPHeaderLen());
		//If  there is an extension
		if (GetX())
		{
			Log("\t\t[Extension type=0x%x len=%d size=%d]\n",GetExtensionType(),GetExtensionLength(),GetExtensionSize());
			if (extension.hasAudioLevel)
				Log("\t\t\t[AudioLevel vad=%d level=%d]\n",GetVAD(),GetLevel());
			if (extension.hasTimeOffset)
				Log("\t\t\t[TimeOffset offset=%d]\n",GetTimeOffset());
			if (extension.hasAbsSentTime)
				Log("\t\t\t[AbsSentTime ts=%lld]\n",GetAbsSendTime());
			Log("\t\t[/Extension]\n");

		}
		::Dump(GetMediaData(),16);
		Log("[[/RTPPacket]\n");
	}
protected:
	BYTE* GetExtensionData()  { return GetX() ? buffer+sizeof(rtp_hdr_t)+4*header->cc+sizeof(rtp_hdr_ext_t) : NULL;		}
public:
	static BYTE  GetType(const BYTE* data)		{ return ((rtp_hdr_t*)data)->pt;		}
	static DWORD GetSSRC(const BYTE* data)		{ return ntohl(((rtp_hdr_t*)data)->ssrc);	}
	static bool  IsRTP(const BYTE* data,DWORD size)	{ return size>=sizeof(rtp_hdr_t) && ((rtp_hdr_t*)data)->version==2;	}
private:
	static const DWORD SIZE = 1700;
private:
	MediaFrame::Type media;
	DWORD	codec;
	DWORD	clockRate;
	WORD	cycles;
	BYTE	buffer[SIZE];
	DWORD	len;
	rtp_hdr_t* header;
	RTPHeaderExtension extension;

};
#endif /* RTPPACKET_H */

