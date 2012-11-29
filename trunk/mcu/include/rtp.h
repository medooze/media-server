#ifndef _RTP_H_
#define _RTP_H_
#include <arpa/inet.h>
#include "config.h"
#include "log.h"
#include "media.h"
#include "video.h"
#include <vector>
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


class RTPPacket
{
public:
	RTPPacket(MediaFrame::Type media,DWORD codec)
	{
		this->media = media;
		this->codec = codec;
		//Get header pointer
		header = (rtp_hdr_t*) buffer;
		//empty header
		memset(header,0,sizeof(rtp_hdr_t));
		//set type
		header->version = 2;
		header->pt = codec;
	}
	
	RTPPacket(MediaFrame::Type media,BYTE *data,DWORD size)
	{
		this->media = media;
		this->codec = codec;
		//Get header pointer
		header = (rtp_hdr_t*) buffer;
		//Set Data
		SetData(data,size);
	}

	RTPPacket(MediaFrame::Type media,DWORD codec,DWORD type)
	{
		this->media = media;
		this->codec = codec;
		//Get header pointer
		header = (rtp_hdr_t*) buffer;
		//empty header
		memset(header,0,sizeof(rtp_hdr_t));
		//set type
		header->version = 2;
		header->pt = type;
	}

	RTPPacket* Clone()
	{
		//New one
		RTPPacket* cloned = new RTPPacket(media,codec,header->pt);
		//Copy
		memcpy(cloned->GetData(),buffer,SIZE);
		//Set media length
		cloned->SetMediaLength(len);
		//Return it
		return cloned;
	}
	
	//Setters
	void SetMediaLength(DWORD len)	{ this->len = len;			}
	void SetTimestamp(DWORD ts)	{ header->ts = htonl(ts);		}
	void SetSeqNum(WORD sn)		{ header->seq = htons(sn);		}
	void SetMark(bool mark)		{ header->m = mark;			}
	void SetCodec(DWORD codec)	{ this->codec = codec;			}
	void SetType(DWORD type)	{ header->pt = type;			}
	void SetSize(DWORD size)	{ len = size-GetRTPHeaderLen();		}
	
	//Getters
	MediaFrame::Type GetMedia()	const { return media;				}
	rtp_hdr_t* GetRTPHeader()	const { return (rtp_hdr_t*)buffer;		}
	DWORD GetRTPHeaderLen()		const { return sizeof(rtp_hdr_t)+4*header->cc;	}
	DWORD GetCodec()		const { return codec;				}
	DWORD GetType()			const { return header->pt;			}
	DWORD GetSize()			const { return len+GetRTPHeaderLen();		}
	BYTE* GetData()			      { return buffer;				}
	DWORD GetMaxSize()		const { return SIZE;				}
	BYTE* GetMediaData()		      { return buffer+GetRTPHeaderLen();	}
	DWORD GetMediaLength()		const { return len;				}
	DWORD GetMaxMediaLength()	const { return SIZE-GetRTPHeaderLen();		}
	bool  GetMark()			const { return header->m;			}
	DWORD GetTimestamp()		const { return ntohl(header->ts);		}
	WORD  GetSeqNum()		const { return ntohs(header->seq);		}
	DWORD GetSSRC()			const { return ntohl(header->ssrc);		}
	

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
private:
	static const DWORD SIZE = 1700;
private:
	MediaFrame::Type media;
	DWORD	codec;
	BYTE	buffer[SIZE];
	DWORD	len;
	rtp_hdr_t* header;
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
	void	SetSendingTime(DWORD sendingTime)	{ this->sendingTime = sendingTime;	}
	DWORD	GetSendingTime()			{ return sendingTime;			}
private:
	DWORD sendingTime;
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
	Type GetType()	{return type; }
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
		virtual DWORD GetSize() = 0;
		virtual DWORD Parse(BYTE* data,DWORD size) = 0;
		virtual DWORD Serialize(BYTE* data,DWORD size) = 0;
	};

	struct NACKField : public Field
	{
		
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
		virtual DWORD GetSize() { return 8;}
		virtual DWORD Parse(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			pid = get4(data,0);
			blp = get4(data,4);
			return 8;
		}
		virtual DWORD Serialize(BYTE* data,DWORD size)
		{
			if (size<8) return 0;
			set4(data,0,pid);
			set4(data,4,blp);
			return 8;
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
			maxTotalBitrateMantissa = maxTotalBitrateMantissa <<8 | data[6] >> 1;
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
			data[6] = maxTotalBitrateMantissa <<1 | (overhead >>8 & 0x01);
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
		WORD GetSSRC() const		{ return ssrc;			}
		WORD GetOverhead() const	{ return overhead;		}
	};
	
public:
	RTCPRTPFeedback();
	virtual ~RTCPRTPFeedback();
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

		static ApplicationLayerFeeedbackField* CreateReceiverEstimatedMaxBitrate(DWORD ssrc,DWORD bitrate)
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
			field->length = 12;
			//Allocate memory
			field->payload = (BYTE*)malloc(field->length);
			//Set id
			field->payload[0] = 'R';
			field->payload[1] = 'E';
			field->payload[2] = 'M';
			field->payload[3] = 'B';
			//Set data
			field->payload[4] = 1;
			field->payload[5] = bitrateExp << 2 | (bitrateMantissa >>16 & 0x03);
			field->payload[6] = bitrateMantissa >> 8;
			field->payload[7] = bitrateMantissa;
			//Set ssrc
			set4(field->payload,8,ssrc);
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

	DWORD GetSize()
	{
		DWORD size = 0;
		//Calculate
		for(RTCPPackets::iterator it = packets.begin(); it!=packets.end(); ++it)
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
	DWORD	    GetPacketCount()			{ return packets.size();	}
	RTCPPacket* GetPacket(DWORD num)		{ return packets[num];		}
	
	static RTCPCompoundPacket* Parse(BYTE *data,DWORD size);
private:
	typedef std::vector<RTCPPacket*> RTCPPackets;
private:
	RTCPPackets packets;
};
#endif
