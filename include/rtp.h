#ifndef _RTP_H_
#define _RTP_H_
#include <arpa/inet.h>
#include "config.h"
#include "media.h"
#include "video.h"

#define RTP_VERSION    2

typedef enum {
    RTCP_SR   = 200,
    RTCP_RR   = 201,
    RTCP_SDES = 202,
    RTCP_BYE  = 203,
    RTCP_APP  = 204
} rtcp_type_t;

typedef enum {
    RTCP_SDES_END   = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_NAME  = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC   = 5,
    RTCP_SDES_TOOL  = 6,
    RTCP_SDES_NOTE  = 7,
    RTCP_SDES_PRIV  = 8
} rtcp_sdes_type_t;

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
    DWORD ts;               /* timestamp */
    DWORD ssrc;             /* synchronization source */
    DWORD csrc[1];          /* optional CSRC list */
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

/*
 * Reception report block
 */
typedef struct {
    DWORD ssrc;             /* data source being reported */
    DWORD fraction:8;	    /* fraction lost since last SR/RR */
    DWORD lost:24;          /* cumul. no. pkts lost (signed!) */
    DWORD last_seq;         /* extended last seq. no. received */
    DWORD jitter;           /* interarrival jitter */
    DWORD lsr;              /* last SR packet from this source */
    DWORD dlsr;             /* delay since last SR packet */
} rtcp_rr_t;

/*
 * SDES item
 */
typedef struct {
    BYTE type;			/* type of item (rtcp_sdes_type_t) */
    BYTE length;		/* length of item (in octets) */
    char data[1];		/* text, not null-terminated */
} rtcp_sdes_item_t;

/*
 * One RTCP packet
 */
typedef struct {
    rtcp_common_t common;     /* common header */
    union {
        /* sender report (SR) */
        struct {
            DWORD ssrc;     /* sender generating this report */
            DWORD ntp_sec;  /* NTP timestamp */
            DWORD ntp_frac;
            DWORD rtp_ts;   /* RTP timestamp */
            DWORD psent;    /* packets sent */
            DWORD osent;    /* octets sent */
            rtcp_rr_t rr[1];  /* variable-length list */
        } sr;

        /* reception report (RR) */
        struct {
            DWORD ssrc;     /* receiver generating this report */
            rtcp_rr_t rr[1];  /* variable-length list */
        } rr;

        /* source description (SDES) */
        struct rtcp_sdes {
            DWORD src;      /* first SSRC/CSRC */
            rtcp_sdes_item_t item[1]; /* list of SDES items */
        } sdes;

        /* BYE */
        struct {
            DWORD src[1];   /* list of sources */
            /* can't express trailing text for reason */
        } bye;
    } r;
} rtcp_t;

typedef struct rtcp_sdes rtcp_sdes_t;


class RTPPacket
{
public:
	RTPPacket(MediaFrame::Type media,DWORD codec,DWORD type)
	{
		this->media = media;
		this->codec = codec;
		this->type = type;
		//Get header pointer
		header = (rtp_hdr_t*) buffer;
		//empty header
		memset(header,0,sizeof(rtp_hdr_t));

	}
	RTPPacket* Clone()
	{
		//New one
		RTPPacket* cloned = new RTPPacket(media,codec,type);
		//Copy
		memcpy(cloned->GetData(),buffer,MTU);
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
	void SetType(DWORD type)	{ this->type = type;			}
	void SetSize(DWORD size)	{ len = size-sizeof(rtp_hdr_t);		}
	
	//Getters
	MediaFrame::Type GetMedia()	{ return media;				}

	DWORD GetCodec()		{ return codec;				}
	DWORD GetType()			{ return type;				}
	DWORD GetSize()			{ return len+sizeof(rtp_hdr_t);		}
	BYTE* GetData()			{ return buffer;			}
	DWORD GetMaxSize()		{ return MTU;				}
	BYTE* GetMediaData()		{ return buffer+sizeof(rtp_hdr_t);	}
	DWORD GetMediaLength()		{ return len;				}
	DWORD GetMaxMediaLength()	{ return RTPPAYLOADSIZE;		}
	bool  GetMark()			{ return header->m;			}
	DWORD GetTimestamp()		{ return ntohl(header->ts);		}
	WORD  GetSeqNum()		{ return ntohs(header->seq);		}
	DWORD GetSSRC()			{ return ntohl(header->ssrc);		}
	rtp_hdr_t* GetRTPHeader()	{ return (rtp_hdr_t*)buffer;		}

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
private:
	MediaFrame::Type media;
	DWORD	type;
	DWORD	codec;
	DWORD	timeStamp;
	BYTE	buffer[MTU];
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

#endif
