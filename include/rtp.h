#ifndef _RTP_H_
#define _RTP_H_
#include <arpa/inet.h>
#include "config.h"
#include "log.h"
#include "media.h"
#include <vector>
#include <list>
#include <math.h>

#define RTP_VERSION    2


/*
 * RTP data header
 */
typedef struct {
    DWORD cc:4;        /* CSRC count */
    DWORD x:1;         /* header extension flag */
    DWORD p:1;         /* padding flag */
    DWORD version:2;   /* protocol version */
    DWORD pt:7;        /* payload type */
    DWORD m:1;         /* marker bit */
    DWORD seq:16;      /* sequence number */
    DWORD ts;          /* timestamp */
    DWORD ssrc;        /* synchronization source */
} rtp_hdr_t;

/* RTP Header Extension
 */
typedef struct {
    WORD ext_type;         /* defined by profile */
    WORD len;              /* extension length in 32-bit word */
} rtp_hdr_ext_t;

/*
 * RTCP common header word
 */
typedef struct {
    DWORD count:5;     /* varies by packet type */
    DWORD p:1;         /* padding flag */
    DWORD version:2;   /* protocol version */
    DWORD pt:8;        /* RTCP packet type */
    DWORD length:16;   /* pkt len in words, w/o this word */
} rtcp_common_t;

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

class RTPPacket
{
public:
	const static DWORD MaxExtSeqNum = 0xFFFFFFFF;
public:
	class HeaderExtension
	{
	friend class  RTPPacket;
	public:
		enum Type {
			SSRCAudioLevel		= 1,
			TimeOffset		= 2,
			AbsoluteSendTime	= 3
		};
	public:
		HeaderExtension()
		{
			absSentTime = 0;
			timeOffset = 0;
			vad = 0;
			level = 0;
			hasAbsSentTime = 0;
			hasTimeOffset =  0;
			hasAudioLevel = 0;
		}
	protected:
		QWORD	absSentTime;
		int	timeOffset;
		bool	vad;
		BYTE	level;
		bool    hasAbsSentTime;
		bool	hasTimeOffset;
		bool	hasAudioLevel;
	};

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
	WORD  GetExtensionType()	const { return GeExtensionHeader()->ext_type;				}
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
	int GetTimeOffset()		const	{ return extension.timeOffset;		}
	bool  GetVAD()			const	{ return extension.vad;			}
	BYTE  GetLevel()		const	{ return extension.level;		}
	bool  HasAudioLevel()		const	{ return extension.hasAudioLevel;	}
	bool  HasAbsSentTime()		const	{ return extension.hasAbsSentTime;	}
	bool  HasTimeOffeset()		const   { return extension.hasTimeOffset;	}

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
		Log("\t[Header v=%d p=%d x=%d cc=%d m=%d pt=%d seq=%d ts=%d ssrc=%d len=%d]\n",GetVersion(),GetP(),GetX(),GetCC(),GetMark(),GetType(),GetSeqNum(),GetClockTimestamp(),GetSSRC(),GetRTPHeaderLen());
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
	HeaderExtension extension;

};

class RTPPacketSched :
	public RTPPacket
{
public:
	RTPPacketSched(MediaFrame::Type media,DWORD codec) : RTPPacket(media,codec,codec)
	{
		//Set sending type
		sendingTime = 0;
	}

	RTPPacketSched(MediaFrame::Type media,BYTE *data,DWORD size) : RTPPacket(media,data,size)
	{
		//Set sending type
		sendingTime = 0;
	}

	RTPPacketSched(MediaFrame::Type media,DWORD codec,DWORD type) : RTPPacket(media,codec,type)
	{
		//Set sending type
		sendingTime = 0;
	}

	void	SetSendingTime(DWORD sendingTime)	{ this->sendingTime = sendingTime;	}
	DWORD	GetSendingTime()			{ return sendingTime;			}
private:
	DWORD sendingTime;
};

class RTPTimedPacket:
	public RTPPacket
{
public:
	RTPTimedPacket(MediaFrame::Type media,DWORD codec) : RTPPacket(media,codec,codec)
	{
		//Set time
		time = ::getTime()/1000;
	}

	RTPTimedPacket(MediaFrame::Type media,BYTE *data,DWORD size) : RTPPacket(media,data,size)
	{
		//Set time
		time = ::getTime()/1000;
	}

	RTPTimedPacket(MediaFrame::Type media,DWORD codec,DWORD type) : RTPPacket(media,codec,type)
	{
		//Set time
		time = ::getTime();
	}

	RTPTimedPacket* Clone()
	{
		//New one
		RTPTimedPacket* cloned = new RTPTimedPacket(GetMedia(),GetCodec(),GetType());
		//Set attrributes
		cloned->SetClockRate(GetClockRate());
		cloned->SetMark(GetMark());
		cloned->SetSeqNum(GetSeqNum());
		cloned->SetSeqCycles(GetSeqCycles());
		cloned->SetTimestamp(GetTimestamp());
		//Set payload
		cloned->SetPayload(GetMediaData(),GetMediaLength());
		//Set time
		cloned->SetTime(GetTime());
		//Check if we have extension
		if (HasAbsSentTime())
			//Set abs time
			cloned->SetAbsSentTime(GetAbsSendTime());
		//Check if we have extension
		if (HasTimeOffeset())
			//Set abs time
			cloned->SetAbsSentTime(GetTimeOffset());
		//Return it
		return cloned;
	}

	QWORD GetTime()	const		{ return time;		}
	void  SetTime(QWORD time )	{ this->time = time;	}
private:
	QWORD time;
};

class RTPRedundantPacket:
	public RTPTimedPacket
{
public:
	RTPRedundantPacket(MediaFrame::Type media,BYTE *data,DWORD size);

	BYTE* GetPrimaryPayloadData() 		const { return primaryData;	}
	DWORD GetPrimaryPayloadSize()		const { return primarySize;	}
	BYTE  GetPrimaryType()			const { return primaryType;	}
	BYTE  GetPrimaryCodec()			const { return primaryCodec;	}
	void  SetPrimaryCodec(BYTE codec)	      { primaryCodec = codec;	}

	RTPTimedPacket* CreatePrimaryPacket();

	BYTE  GetRedundantCount()		const { return headers.size();	}
	BYTE* GetRedundantPayloadData(int i)	const { return i<headers.size()?redundantData+headers[i].ini:NULL;	}
	DWORD GetRedundantPayloadSize(int i) 	const { return i<headers.size()?headers[i].size:0;			}
	BYTE  GetRedundantType(int i)		const { return i<headers.size()?headers[i].type:0;			}
	BYTE  GetRedundantCodec(int i)		const { return i<headers.size()?headers[i].codec:0;			}
	BYTE  GetRedundantOffser(int i)		const { return i<headers.size()?headers[i].offset:0;			}
	BYTE  GetRedundantTimestamp(int i)	const { return i<headers.size()?GetTimestamp()-headers[i].offset:0;	}
	void  SetRedundantCodec(int i,BYTE codec)     { if (i<headers.size()) headers[i].codec = codec;			}

private:
	struct RedHeader
	{
		BYTE  type;
		BYTE  codec;
		DWORD offset;
		DWORD ini;
		DWORD size;
		RedHeader(BYTE type,DWORD offset,DWORD ini,DWORD size)
		{
			this->codec = type;
			this->type = type;
			this->offset = offset;
			this->ini = ini;
			this->size = size;
		}
	};
private:
	std::vector<RedHeader> headers;
	BYTE	primaryType;
	BYTE	primaryCodec;
	DWORD	primarySize;
	BYTE*	primaryData;
	BYTE*	redundantData;
};

class RTPDepacketizer
{
public:
	static RTPDepacketizer* Create(MediaFrame::Type mediaType,DWORD codec);

public:
	RTPDepacketizer(MediaFrame::Type mediaType,DWORD codec)
	{
		//Store
		this->mediaType = mediaType;
		this->codec = codec;
	}
	virtual ~RTPDepacketizer()	{};

	MediaFrame::Type GetMediaType()	{ return mediaType; }
	DWORD		 GetCodec()	{ return codec; }

	virtual void SetTimestamp(DWORD timestamp) = 0;
	virtual MediaFrame* AddPacket(RTPPacket *packet) = 0;
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len) = 0;
	virtual void ResetFrame() = 0;
private:
	MediaFrame::Type	mediaType;
	DWORD			codec;
};

class RTCPReport
{
public:
	DWORD GetSSRC()			const { return get4(buffer,0);  }
	BYTE  GetFactionLost()		const { return get1(buffer,4);  }
	DWORD GetLostCount()		const { return get3(buffer,5);  }
	DWORD GetLastSeqNum()		const { return get4(buffer,8);  }
	DWORD GetJitter()		const { return get4(buffer,12); }
	DWORD GetLastSR()		const { return get4(buffer,16); }
	DWORD GetDelaySinceLastSR()	const { return get4(buffer,20); }
	DWORD GetSize()			const { return 24;		}

	void SetSSRC(DWORD ssrc)		{ set4(buffer,0,ssrc);		}
	void SetFractionLost(BYTE fraction)	{ set1(buffer,4,fraction);	}
	void SetLostCount(DWORD count)		{ set3(buffer,5,count);		}
	void SetLastSeqNum(DWORD seq)		{ set4(buffer,8,seq);		}
	void SetLastJitter(DWORD jitter)	{ set4(buffer,12,jitter);	}
	void SetLastSR(DWORD last)		{ set4(buffer,16,last);		}
	void SetDelaySinceLastSR(DWORD delay)	{ set4(buffer,20,delay);	}

	void SetDelaySinceLastSRMilis(DWORD milis)
	{
		//calculate the delay, expressed in units of 1/65536 seconds
		DWORD dlsr = (milis/1000) << 16 | (DWORD)((milis%1000)*65.535);
		//Set it
		SetDelaySinceLastSR(dlsr);
	}

	DWORD GetDelaySinceLastSRMilis()
	{
		//Get the delay, expressed in units of 1/65536 seconds
		DWORD dslr = GetDelaySinceLastSR();
		//Return in milis
		return (dslr>>16)*1000 + ((double)(dslr & 0xFFFF))/65.635;
	}


	DWORD Serialize(BYTE* data,DWORD size)
	{
		//Check size
		if (size<24)
			return 0;
		//Copy
		memcpy(data,buffer,24);
		//Return size
		return 24;
	}

	DWORD Parse(BYTE* data,DWORD size)
	{
		//Check size
		if (size<24)
			return 0;
		//Copy
		memcpy(buffer,data,24);
		//Return size
		return 24;
	}
	void Dump()
	{
		Debug("\t\t[Report ssrc=%u\n",		GetSSRC());
		Debug("\t\t\tfractionLost=%u\n",	GetFactionLost());
		Debug("\t\t\tlostCount=%u\n",		GetLostCount());
		Debug("\t\t\tlastSeqNum=%u\n",		GetLastSeqNum());
		Debug("\t\t\tjitter=%u\n",		GetJitter());
		Debug("\t\t\tlastSR=%u\n",		GetLastSR());
		Debug("\t\t\tdelaySinceLastSR=%u\n",	GetDelaySinceLastSR());
		Debug("\t\t/]");
	}
private:
	BYTE buffer[24];
};


class RTCPPacket
{
public:
	 enum Type{
		FullIntraRequest	= 192,
		NACK			= 193,
		ExtendedJitterReport	= 195,
		SenderReport		= 200,
		ReceiverReport		= 201,
		SDES			= 202,
		Bye			= 203,
		App			= 204,
		RTPFeedback		= 205,
		PayloadFeedback		= 206
	};

	static const char* TypeToString(Type type)
	{
		switch (type)
		{
			case RTCPPacket::SenderReport:
				return "SenderReport";
			case RTCPPacket::ReceiverReport:
				return "ReceiverReport";
			case RTCPPacket::SDES:
				return "SDES";
			case RTCPPacket::Bye:
				return "Bye";
			case RTCPPacket::App:
				return "App";
			case RTCPPacket::RTPFeedback:
				return "RTPFeedback";
			case RTCPPacket::PayloadFeedback:
				return "PayloadFeedback";
			case RTCPPacket::FullIntraRequest:
				return "FullIntraRequest";
			case RTCPPacket::NACK:
				return "NACK";
		}
		return("Unknown");
	}
protected:
	RTCPPacket(Type type)
	{
		this->type = type;
	}
public:
	// Must have virtual destructor to ensure child class's destructor is called
	virtual ~RTCPPacket(){};
	Type GetType() const {return type; }
	virtual void Dump();
	virtual DWORD GetSize() = 0;
	virtual DWORD Parse(BYTE* data,DWORD size) = 0;
	virtual DWORD Serialize(BYTE* data,DWORD size) = 0;
protected:
	typedef std::vector<RTCPReport*> Reports;
private:
	Type type;
};

class RTCPSenderReport : public RTCPPacket
{
public:
	RTCPSenderReport();
	virtual ~RTCPSenderReport();
	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	void SetOctectsSent(DWORD octectsSent)	{ this->octectsSent = octectsSent;	}
	void SetPacketsSent(DWORD packetsSent)	{ this->packetsSent = packetsSent;	}
	void SetRtpTimestamp(DWORD rtpTimestamp){ this->rtpTimestamp = rtpTimestamp;	}
	void SetSSRC(DWORD ssrc)		{ this->ssrc = ssrc;			}
	void SetNTPFrac(DWORD ntpFrac)		{ this->ntpFrac = ntpFrac;		}
	void SetNTPSec(DWORD ntpSec)		{ this->ntpSec = ntpSec;		}

	DWORD GetOctectsSent()	const		{ return octectsSent;		}
	DWORD GetPacketsSent()	const		{ return packetsSent;		}
	DWORD GetRTPTimestamp() const		{ return rtpTimestamp;		}
	DWORD GetNTPFrac()	const		{ return ntpFrac;		}
	DWORD GetNTPSec()	const		{ return ntpSec;		}
	QWORD GetNTPTimestamp()	const		{ return ((QWORD)ntpSec)<<32 | ntpFrac ;	}
	DWORD GetSSRC()		const		{ return ssrc;			}

	DWORD GetCount()	const		{ return reports.size();	}
	RTCPReport* GetReport(BYTE i) const	{ return reports[i];		}
	void  AddReport(RTCPReport* report)	{ reports.push_back(report);	}

	void  SetTimestamp(timeval *tv);
	QWORD GetTimestamp() const;
	void  GetTimestamp(timeval *tv) const;

private:
	DWORD ssrc;           /* sender generating this report */
	DWORD ntpSec;	      /* NTP timestamp */
	DWORD ntpFrac;
	DWORD rtpTimestamp;   /* RTP timestamp */
	DWORD packetsSent;    /* packets sent */
	DWORD octectsSent;    /* octets sent */

private:
	Reports	reports;
};

class RTCPReceiverReport : public RTCPPacket
{
public:
	RTCPReceiverReport();
	virtual ~RTCPReceiverReport();
	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	DWORD GetCount()	const		{ return reports.size();	}
	RTCPReport* GetReport(BYTE i) const	{ return reports[i];		}
	void  AddReport(RTCPReport* report)	{ reports.push_back(report);	}
private:
	DWORD ssrc;     /* receiver generating this report */

private:
	Reports	reports;
};

class RTCPBye : public RTCPPacket
{
public:
	RTCPBye();
	~RTCPBye();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	typedef std::vector<DWORD> SSRCs;
private:
	SSRCs ssrcs;
	char* reason;
};

class RTCPExtendedJitterReport : public RTCPPacket
{
public:
	RTCPExtendedJitterReport();

	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	typedef std::vector<DWORD> Jitters;
private:
	Jitters jitters;
};

class RTCPFullIntraRequest : public RTCPPacket
{
public:
	RTCPFullIntraRequest();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	DWORD ssrc;
};

class RTCPNACK : public RTCPPacket
{
public:
	RTCPNACK();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	DWORD ssrc;
	WORD fsn;
	WORD blp;
};

class RTCPSDES : public RTCPPacket
{
public:

public:
	class Item
	{
	public:
		enum Type
		{
			CName = 1,
			Name = 2,
			Email = 3,
			Phone = 4,
			Location = 5,
			Tool = 6,
			Note = 7,
			Private = 8
		};
		static const char* TypeToString(Type type)
		{
			switch(type)
			{
				case CName:
					return "CName";
				case Name:
					return "Name";
				case Email:
					return "Email";
				case Phone:
					return "Phone";
				case Location:
					return "Location";
				case Tool:
					return "Tool";
				case Note:
					return "Note";
				case Private:
					return "Private";
			}
			return "Unknown";
		}
	public:
		Item(Type type,BYTE* data,DWORD size)
		{
			this->type = type;
			this->data = (BYTE*)malloc(size);
			this->size = size;
			memcpy(this->data,data,size);
		}
		Item(Type type,const char* str)
		{
			this->type = type;
			size = strlen(str);
			data = (BYTE*)malloc(size);
			memcpy(data,(BYTE*)str,size);
		}
		~Item()
		{
			free(data);
		}
		Type  GetType() const { return type; }
		BYTE* GetData() const { return data; }
		BYTE  GetSize() const { return size; }
	private:
		Type type;
		BYTE* data;
		BYTE size;
	};

	class Description
	{
	public:
		Description();
		Description(DWORD ssrc);
		~Description();
		void Dump();
		DWORD GetSize();
		DWORD Parse(BYTE* data,DWORD size);
		DWORD Serialize(BYTE* data,DWORD size);

		void AddItem(Item* item)	{ items.push_back(item);	}
		DWORD GetItemCount() const	{ return items.size();		}
		Item* GetItem(BYTE i)		{ return items[i];		}

		DWORD GetSSRC()	const		{ return ssrc;			}
		void  SetSSRC(DWORD ssrc)	{ this->ssrc = ssrc;		}
	private:
		typedef std::vector<Item*> Items;
	private:
		DWORD ssrc;
		Items items;
	};
public:
	RTCPSDES();
	~RTCPSDES();
	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	void AddDescription(Description* desc)	{ descriptions.push_back(desc);	}
	DWORD GetDescriptionCount() const	{ return descriptions.size();	}
	Description* GetDescription(BYTE i)	{ return descriptions[i];	}
private:
	typedef std::vector<Description*> Descriptions;
private:
	Descriptions descriptions;
};

class RTCPApp : public RTCPPacket
{
public:
	RTCPApp();
	virtual ~RTCPApp();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
private:
	BYTE subtype;
	DWORD ssrc;
	char name[4];
	BYTE *data;
	DWORD size;
};

class RTCPRTPFeedback : public RTCPPacket
{
public:
	enum FeedbackType {
		NACK = 1,
		TempMaxMediaStreamBitrateRequest = 3,
		TempMaxMediaStreamBitrateNotification =4
	};

	static const char* TypeToString(FeedbackType type)
	{
		switch(type)
		{
			case NACK:
				return "NACK";
			case TempMaxMediaStreamBitrateRequest:
				return "TempMaxMediaStreamBitrateRequest";
			case TempMaxMediaStreamBitrateNotification:
				return "TempMaxMediaStreamBitrateNotification";
		}
		return "Unknown";
	}

	static RTCPRTPFeedback* Create(FeedbackType type,DWORD senderSSRC,DWORD mediaSSRC)
	{
		//Create
		RTCPRTPFeedback* packet = new RTCPRTPFeedback();
		//Set type
		packet->SetFeedBackTtype(type);
		//Set ssrcs
		packet->SetSenderSSRC(senderSSRC);
		packet->SetMediaSSRC(mediaSSRC);
		//return it
		return packet;
	}
public:
	struct Field
	{
		virtual ~Field(){};
		virtual DWORD GetSize() = 0;
		virtual DWORD Parse(BYTE* data,DWORD size) = 0;
		virtual DWORD Serialize(BYTE* data,DWORD size) = 0;
	};

	struct NACKField : public Field
	{

		/*
		 *
		    0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |            PID                |             BLP               |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */

		WORD pid;
		WORD blp;

		NACKField()
		{
			pid = 0;
			blp = 0;
		}
		NACKField(WORD pid,WORD blp)
		{
			this->pid = pid;
			this->blp = blp;
		}
		NACKField(WORD pid,BYTE blp[2])
		{
			this->pid = pid;
			this->blp = get2(blp,0);
		}
		virtual DWORD GetSize() { return 4;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<4) return 0;
			pid = get2(data,0);
			blp = get2(data,2);
			return 4;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<4) return 0;
			set2(data,0,pid);
			set2(data,2,blp);
			return 4;
		}
	};

	struct TempMaxMediaStreamBitrateField : public Field
	{
		/*
		    0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                              SSRC                             |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */
		DWORD	ssrc;
		BYTE	maxTotalBitrateExp;		// 6 bits
		DWORD	maxTotalBitrateMantissa;	// 17 bits
		WORD	overhead;			// 9 bits

		TempMaxMediaStreamBitrateField()
		{
			ssrc = 0;
			maxTotalBitrateExp = 0;
			maxTotalBitrateMantissa = 0;
			overhead = 0;
		}
		TempMaxMediaStreamBitrateField(DWORD ssrc,DWORD bitrate,WORD overhead)
		{
			this->ssrc = ssrc;
			this->overhead = overhead;
			SetBitrate(bitrate);
		}
		virtual DWORD GetSize() { return 8;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			ssrc = get4(data,0);
			maxTotalBitrateExp	= data[4] >> 2;
			maxTotalBitrateMantissa = data[4] & 0x03;
			maxTotalBitrateMantissa = maxTotalBitrateMantissa <<8 | data[5];
			maxTotalBitrateMantissa = maxTotalBitrateMantissa <<7 | data[6] >> 1;
			overhead		= data[6] & 0x01;
			overhead		= overhead << 8 | data[7];
			return 8;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			set4(data,0,ssrc);
			data[4] = maxTotalBitrateExp << 2 | (maxTotalBitrateMantissa >>15 & 0x03);
			data[5] = maxTotalBitrateMantissa >>7;
			data[6] = maxTotalBitrateMantissa <<1 | (overhead>>8 & 0x01);
			data[7] = overhead;
			return 8;
		}

		void SetBitrate(DWORD bitrate)
		{
			//Init exp
			maxTotalBitrateExp = 0;
			//Find 17 most significants bits
			for(BYTE i=0;i<64;i++)
			{
				//If bitrate is less than
				if(bitrate <= (0x001FFFF << i))
				{
					//That's the exp
					maxTotalBitrateExp = i;
					break;
				}
			}
			//Get mantisa
			maxTotalBitrateMantissa = (bitrate >> maxTotalBitrateExp);
		}

		DWORD GetBitrate() const
		{
			return maxTotalBitrateMantissa << maxTotalBitrateExp;
		}

		void SetSSRC(WORD ssrc)		{ this->ssrc = ssrc;		}
		void SetOverhead(WORD overhead)	{ this->overhead = overhead;	}
		DWORD GetSSRC() const		{ return ssrc;			}
		WORD GetOverhead() const	{ return overhead;		}
	};

public:
	RTCPRTPFeedback();
	virtual ~RTCPRTPFeedback();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);
	virtual void Dump();

	void SetSenderSSRC(DWORD ssrc)		{ senderSSRC = ssrc;		}
	void SetMediaSSRC(DWORD ssrc)		{ mediaSSRC = ssrc;		}
	void SetFeedBackTtype(FeedbackType type){ feedbackType = type;		}
	void AddField(Field* field)		{ fields.push_back(field);	}

	DWORD		GetMediaSSRC() const	{ return mediaSSRC;		}
	DWORD		GetSenderSSRC() const	{ return senderSSRC;		}
	FeedbackType	GetFeedbackType() const	{ return feedbackType;		}
	DWORD		GetFieldCount() const	{ return fields.size();		}
	Field*		GetField(BYTE i) const	{ return fields[i];		}

private:
	typedef std::vector<Field*> Fields;
private:
	FeedbackType feedbackType;
	DWORD senderSSRC;
	DWORD mediaSSRC;
	Fields fields;
};

class RTCPPayloadFeedback : public RTCPPacket
{
public:
	enum FeedbackType {
		PictureLossIndication =1,
		SliceLossIndication = 2,
		ReferencePictureSelectionIndication = 3,
		FullIntraRequest = 4,
		TemporalSpatialTradeOffRequest = 5,
		TemporalSpatialTradeOffNotification = 6,
		VideoBackChannelMessage = 7,
		ApplicationLayerFeeedbackMessage = 15
	};

	static const char* TypeToString(FeedbackType type)
	{
		switch(type)
		{
			case PictureLossIndication:
				return "PictureLossIndication";
			case SliceLossIndication:
				return "SliceLossIndication";
			case ReferencePictureSelectionIndication:
				return "ReferencePictureSelectionIndication";
			case FullIntraRequest:
				return "FullIntraRequest";
			case TemporalSpatialTradeOffRequest:
				return "TemporalSpatialTradeOffRequest";
			case TemporalSpatialTradeOffNotification:
				return "TemporalSpatialTradeOffNotification";
			case VideoBackChannelMessage:
				return "VideoBackChannelMessage";
			case ApplicationLayerFeeedbackMessage:
				return "ApplicationLayerFeeedbackMessage";
		}
		return "Unknown";
	}

	static RTCPPayloadFeedback* Create(FeedbackType type,DWORD senderSSRC,DWORD mediaSSRC)
	{
		//Create
		RTCPPayloadFeedback* packet = new RTCPPayloadFeedback();
		//Set type
		packet->SetFeedBackTtype(type);
		//Set ssrcs
		packet->SetSenderSSRC(senderSSRC);
		packet->SetMediaSSRC(mediaSSRC);
		//return it
		return packet;
	}
public:
	struct Field
	{
		virtual ~Field(){};
		virtual DWORD GetSize() = 0;
		virtual DWORD Parse(BYTE* data,DWORD size) = 0;
		virtual DWORD Serialize(BYTE* data,DWORD size) = 0;
	};

	struct SliceLossIndicationField : public Field
	{
		/*
		    0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |            First        |        Number           | PictureID |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */
		WORD first; // 13 b
		WORD number; // 13 b
		BYTE pictureId; // 6
		virtual DWORD GetSize() { return 4;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<4) return 0;
			first = data[0];
			first = first<<5 | data [1]>>3;
			number = data[1] & 0x07;
			number = number<<8 | data[2];
			number = number<<2 | data[3]>>6;
			pictureId = data[4] & 0x3F;
			return 4;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<4) return 0;
			data[0] = first >> 5;
			data[1] = first << 3 | number >> 10;
			data[2] = number >> 2;
			data[3] = number << 6 | pictureId;
			return 4;
		}
	};

	struct ReferencePictureSelectionField : public Field
	{
		/*
		    0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |      PB       |0| Payload Type|    Native RPSI bit string     |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |   defined per codec          ...                | Padding (0) |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 *
		 */
		BYTE padding;
		BYTE type;
		DWORD length;
		BYTE* payload;

		ReferencePictureSelectionField()
		{
			payload = NULL;
			length = 0;
		}

		~ReferencePictureSelectionField()
		{
			if (payload) free(payload);
		}

		virtual DWORD GetSize() { return 2+length+padding;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<2) return 0;
			//Get values
			padding = data[0];
			type = data[1];
			if (size<2+padding) return 0;
			//Set length
			length = size-padding-2;
			//allocate
			payload = (BYTE*)malloc(length);
			//Copy payload
			memcpy(payload,data+2,length);
			//Copy
			return 2+padding+length;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<2+padding+length) return 0;
			//Set
			data[0] = padding;
			data[1] = type & 0x7F;
			set2(data,6,length);
			//copy payload
			memcpy(data+2,payload,length);
			//Fill padding
			memset(data+2+length,0,padding);
			//return size
			return 2+padding+length;
		}
	};

	struct FullIntraRequestField : public Field
	{
		/*
		    0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                              SSRC                             |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   | Seq. nr       |    Reserved                                   |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		*/
		DWORD ssrc;
		BYTE seq;

		FullIntraRequestField()
		{
			this->ssrc = 0;
			this->seq  = 0;
		}

		FullIntraRequestField(DWORD ssrc,BYTE seq)
		{
			this->ssrc = ssrc;
			this->seq  = seq;
		}

		virtual DWORD GetSize() { return 8;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			ssrc = get4(data,0);
			seq = data[5];
			return 8;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			set4(data,0,ssrc);
			data[4] = seq;
			data[5] = 0;
			data[6] = 0;
			data[7] = 0;
			return 8;
		}
	};

	struct TemporalSpatialTradeOffField : public Field
	{
		/*
		 *  0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                              SSRC                             |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |  Seq nr.      |  Reserved                           | Index   |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */
		DWORD ssrc;
		BYTE seq;
		BYTE index;
		virtual DWORD GetSize() { return 8;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			//Get values
			ssrc = get4(data,0);
			seq = data[4];
			index = data[7];
			//Return size
			return 8;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			//Set values
			set4(data,0,ssrc);
			data[4] = seq;
			data[5] = 0;
			data[6] = 0;
			data[7] = index;
			//Return size
			return 8;
		}
	};

	struct VideoBackChannelMessageField : public Field
	{
		/*
		    0                   1                   2                   3
		    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                              SSRC                             |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   | Seq. nr       |0| Payload Type| Length                        |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                    VBCM Octet String....      |    Padding    |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */
		DWORD ssrc;
		BYTE seq;
		BYTE type;
		WORD length;
		BYTE* payload;
		VideoBackChannelMessageField()
		{
			payload = 0;
			length = 0;
		}
		~VideoBackChannelMessageField()
		{
			if (payload) free(payload);
		}
		virtual DWORD GetSize() { return 8+length;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			//Get values
			ssrc = get4(data,0);
			seq = data[4];
			type = data[5];
			length = get2(data,6);
			if (size<8+pad32(length)) return 0;
			//allocate
			payload = (BYTE*)malloc(length);
			//Copy payload
			memcpy(payload,data+8,length);
			//Copy
			return 8+pad32(length);
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<8+pad32(length)) return 0;
			//Set values
			set4(data,0,ssrc);
			data[4] = seq;
			data[5] = type & 0x7F;
			set2(data,6,length);
			//copy payload
			memcpy(data+8,payload,length);
			//Fill padding
			memset(data+8+length,0,pad32(length)-length);
			//return size
			return 8+pad32(length);
		}
	};

	struct ApplicationLayerFeeedbackField : public Field
	{
		WORD length;
		BYTE* payload;
		ApplicationLayerFeeedbackField()
		{
			payload = 0;
			length = 0;
		}
		~ApplicationLayerFeeedbackField()
		{
			if (payload) free(payload);
		}
		const BYTE* GetPayload() const	{ return payload;	}
		DWORD GetLength() const	{ return length;	}
		virtual DWORD GetSize() { return pad32(length); }
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size!=pad32(size)) return 0;
			//Get values
			length = size;
			//allocate
			payload = (BYTE*)malloc(length);
			//Copy payload
			memcpy(payload,data,length);
			//Copy
			return length;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<pad32(length)) return 0;
			//copy payload
			memcpy(data,payload,length);
			//Fill padding
			memset(data+length,0,pad32(length)-length);
			//return size
			return pad32(length);
		}

		static ApplicationLayerFeeedbackField* CreateReceiverEstimatedMaxBitrate(std::list<DWORD> ssrcs,DWORD bitrate)
		{
			/*
			    0                   1                   2                   3
			    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			   |  Unique identifier 'R' 'E' 'M' 'B'                            |
			   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			   |  Num SSRC     | BR Exp    |  BR Mantissa (18)                 |
			   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			   |   SSRC feedback                                               |
			   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 **/

			//Init exp
			BYTE bitrateExp = 0;
			//Find 18 most significants bits
			for(BYTE i=0;i<64;i++)
			{
				//If bitrate is less than
				if(bitrate <= (0x003FFFF << i))
				{
					//That's the exp
					bitrateExp = i;
					break;
				}
			}
			//Get mantisa
			DWORD bitrateMantissa = (bitrate >> bitrateExp);
			//Create field
			ApplicationLayerFeeedbackField* field = new ApplicationLayerFeeedbackField();
			//Set size of data
			field->length = 8+4*ssrcs.size();
			//Allocate memory
			field->payload = (BYTE*)malloc(field->length);
			//Set id
			field->payload[0] = 'R';
			field->payload[1] = 'E';
			field->payload[2] = 'M';
			field->payload[3] = 'B';
			//Set data
			field->payload[4] = ssrcs.size();
			field->payload[5] = bitrateExp << 2 | (bitrateMantissa >>16 & 0x03);
			field->payload[6] = bitrateMantissa >> 8;
			field->payload[7] = bitrateMantissa;
			//Num of ssrcs
			DWORD i = 0;
			//For each ssrc
			for (std::list<DWORD>::iterator it = ssrcs.begin(); it!= ssrcs.end(); ++it)
				//Set ssrc
				set4(field->payload,8+4*(i++),(*it));
			//Return it
			return field;
		}

	};
public:
	RTCPPayloadFeedback();
	virtual ~RTCPPayloadFeedback();

	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	void SetSenderSSRC(DWORD ssrc)		{ senderSSRC = ssrc;		}
	void SetMediaSSRC(DWORD ssrc)		{ mediaSSRC = ssrc;		}
	void SetFeedBackTtype(FeedbackType type){ feedbackType = type;		}
	void AddField(Field* field)		{ fields.push_back(field);	}

	DWORD		GetMediaSSRC() const	{ return mediaSSRC;		}
	DWORD		GetSenderSSRC() const	{ return senderSSRC;		}
	FeedbackType	GetFeedbackType() const	{ return feedbackType;		}
	DWORD		GetFieldCount() const	{ return fields.size();		}
	Field*		GetField(BYTE i) const	{ return fields[i];		}
private:
	typedef std::vector<Field*> Fields;
private:
	FeedbackType feedbackType;
	DWORD senderSSRC;
	DWORD mediaSSRC;
	Fields fields;
};

class RTCPCompoundPacket
{
public:
	static bool IsRTCP(BYTE *data,DWORD size)
	{
		//Check size
		if (size<sizeof(rtcp_common_t))
			//No
			return 0;
		//Get RTCP common header
		rtcp_common_t* header = (rtcp_common_t*)data;
		//Check version
		if (header->version!=2)
			//No
			return 0;
		//Check type
		if (header->pt<200 ||  header->pt>206)
			//It is no
			return 0;
		//RTCP
		return 1;
	}

	~RTCPCompoundPacket()
	{
		//Fir each oen
		for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
			//delete
			delete((*it));
		//Clean
		packets.clear();
	}

	DWORD GetSize() const
	{
		DWORD size = 0;
		//Calculate
		for(RTCPPackets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
			//Append size
			size = sizeof(rtcp_common_t)+(*it)->GetSize();
		//Return total size
		return size;
	}

	DWORD Serialize(BYTE *data,DWORD size)
	{
		DWORD len = 0;
		//Check size
		if (size<GetSize())
			//Error
			return 0;
		//For each one
		for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
			//Serialize
			len +=(*it)->Serialize(data+len,size-len);
		//Exit
		return len;
	}
	void Dump();

	void	    AddRTCPacket(RTCPPacket* packet)	{ packets.push_back(packet);	}
	DWORD	    GetPacketCount()	const		{ return packets.size();	}
	const RTCPPacket* GetPacket(DWORD num) const		{ return packets[num];		}

	static RTCPCompoundPacket* Parse(BYTE *data,DWORD size);
private:
	typedef std::vector<RTCPPacket*> RTCPPackets;
private:
	RTCPPackets packets;
};

struct RTPSource 
{
	DWORD	SSRC;
	DWORD   extSeq;
	DWORD	cycles;
	DWORD	jitter;
	DWORD	numPackets;
	DWORD	numRTCPPackets;
	DWORD	totalBytes;
	DWORD	totalRTCPBytes;
	
	RTPSource()
	{
		SSRC		= random();
		extSeq		= random();
		cycles		= 0;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
		jitter		= 0;
	}
	
	RTCPCompoundPacket* CreateSenderReport();
	
	virtual void Reset()
	{
		SSRC		= random();
		extSeq		= random();
		cycles		= 0;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
		jitter		= 0;
	}
};

struct RTPIncomingSource : public RTPSource
{
	DWORD	lostPackets;
	DWORD	totalPacketsSinceLastSR;
	DWORD   nackedPacketsSinceLastSR;
	DWORD	totalBytesSinceLastSR;
	DWORD	minExtSeqNumSinceLastSR ;
	DWORD   lostPacketsSinceLastSR;
	
	RTPIncomingSource() : RTPSource()
	{
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		nackedPacketsSinceLastSR = 0;
		totalBytesSinceLastSR	 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		lostPackets		 = 0;
		totalPacketsSinceLastSR	 = 0;
		nackedPacketsSinceLastSR = 0;
		totalBytesSinceLastSR	 = 0;
		minExtSeqNumSinceLastSR  = RTPPacket::MaxExtSeqNum;
	}
};

struct RTPOutgoingSource : public RTPSource
{
	DWORD   time;
	DWORD   lastTime;
	DWORD	numPackets;
	DWORD	numRTCPPackets;
	DWORD	totalBytes;
	DWORD	totalRTCPBytes;
	
	RTPOutgoingSource() : RTPSource()
	{
		time		= random();
		lastTime	= time;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
	}
	
	RTCPSenderReport* CreateSenderReport(timeval *tv)
	{
		//Create Sender report
		RTCPSenderReport *sr = new RTCPSenderReport();

		//Append data
		sr->SetSSRC(SSRC);
		sr->SetTimestamp(tv);
		sr->SetRtpTimestamp(lastTime);
		sr->SetOctectsSent(totalBytes);
		sr->SetPacketsSent(numPackets);
		
		//Return it
		return sr;
	}
	
	virtual void Reset()
	{
		RTPSource::Reset();
		time		= random();
		lastTime	= time;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
	}
	
};

struct RTPIncomingRtxSource : public RTPIncomingSource
{
	int apt;
	RTPIncomingSource* original;

	RTPIncomingRtxSource() : RTPIncomingSource()
	{
		apt = -1;
		original = NULL;
	}
};

struct RTPOutgoingRtxSource : public RTPOutgoingSource
{
	int apt;
	RTPIncomingSource* original;

	RTPOutgoingRtxSource() : RTPOutgoingSource()
	{
		apt = -1;
		original = NULL;
	}
};

class RTPLostPackets
{
public:
	RTPLostPackets(WORD num);
	~RTPLostPackets();
	void Reset();
	WORD AddPacket(const RTPTimedPacket *packet);
	std::list<RTCPRTPFeedback::NACKField*>  GetNacks();
	void Dump();
	
private:
	QWORD *packets;
	WORD size;
	WORD len;
	DWORD first;
};

#endif
