#ifndef _RTMP_H_
#define _RTMP_H_
#include <string.h>
#include "config.h"
#include "tools.h"
#include "log.h"

class RTMPObject
{
public:
	virtual ~RTMPObject() = default;
	virtual DWORD Parse(BYTE *buffer,DWORD bufferSize) = 0;
	virtual bool IsParsed() = 0;
	virtual DWORD GetSize() = 0;
	virtual DWORD Serialize(BYTE *data,DWORD size) = 0;
	virtual void Dump() = 0;
};

//Probably should not have used templates here.. 
template <int SIZE>
class RTMPFormat : public RTMPObject
{
public:
	RTMPFormat()
	{
		len = 0;
		size = SIZE;
	}
	
	void Reset()
	{
		len = 0;
		size = SIZE;
	}

	virtual void PreParse(BYTE *buffer,DWORD bufferSize)
	{
	}

	virtual DWORD Parse(BYTE *buffer,DWORD bufferSize)
	{
		DWORD num;

		//Sanity check	
		if (bufferSize==0)
			//Exit
			return 0;

		//Preparse
		PreParse(buffer,bufferSize);
	
		//Check sizes
		if (bufferSize>size-len)
			//Fill
			num = size-len;
		else
			//All
			num = bufferSize;

		//Copy data to header
		memcpy(data+len,buffer,num);

		//Increse size
		len += num;

		//Return bytes used
		return num;
	}

	BYTE* GetData()
	{
		return data;
	}

	DWORD GetSize()
	{
		return size;
	}

	virtual bool IsParsed()
	{
		return (len==size);
	}
		
	virtual void Dump()
	{
		::Dump(data,size);
	}
	
	DWORD Serialize(BYTE* buffer,DWORD bufferSize)
	{	
		if (bufferSize<size)
			return -1;
		memcpy(buffer,data,size);
		return size;
	}
	
protected:
	BYTE	data[SIZE] = {};
	DWORD	len;
	DWORD 	size;
};

/***********************************************************
 * Message Header
 * The message header contains the following:
 * Message Type:
 * 	One byte field to represent the message type. A range of type IDs
 * 	(1-7) are reserved for protocol control messages.
 * Length:
 * 	Three-byte field that represents the size of the payload in bytes.
 * 	It is set in big-endian format.
 * Timestamp:
 * 	Four-byte field that contains a timestamp of the message.
 * 	The 4 bytes are packed in the big-endian order.
 * Message Stream Id:
 * 	Three-byte field that identifies the stream of the message. These
 * 	bytes are set in big-endian format.
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Message Type|               Payload length                  |
 * | (1 byte)    |                  (3 bytes)                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Timestamp                           |
 * |                         (4 bytes)                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Stream ID                |    
 * |                       (3 bytes)               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 ***********************************************************/
class RTMPMessageHeader : public RTMPFormat<11>
{
public:
	DWORD 	GetMessageType() 	{ return get3(data,0); }
	DWORD 	GetPayloadLength() 	{ return get4(data,3); }
	BYTE 	GetTimestamp()		{ return get1(data,7); }
	DWORD 	GetStreamId() 		{ return get3(data,8); }

        void    SetMessageType(DWORD messageType)	{ set1(data,0,messageType); }
	void	SetPayloadLength(DWORD payloadLength) 	{ set3(data,1,payloadLength); }
	void	SetTimestamp(BYTE timestamp)		{ set4(data,4,timestamp); }
	void	SetStreamId(DWORD streamId)		{ set3(data,8,streamId); }

	virtual void Dump()
	{
		Debug("Header [type:%d,lenght:%d,timestamp:%d,id:%d]\n",GetMessageType(),GetPayloadLength(),GetTimestamp(),GetStreamId());
	}
};

/************************************************************
 * The Chunk Basic Header encodes the chunk stream ID and the chunk
 * type(represented by fmt field in the figure below). Chunk type
 * determines the format of the encoded message header. Chunk Basic
 * Header field may be 1, 2, or 3 bytes, depending on the chunk stream
 * ID.
 *
 * Chunk stream IDs 2-63 can be encoded in the 1-byte version of this
 * field.
 * 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+
 * |fmt|   cs id   |
 * +-+-+-+-+-+-+-+-+
 *
 * Chunk stream IDs 64-319 can be encoded in the 2-byte version of this
 * field. ID is computed as (the second byte + 64).
 *
 * 0                   1
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |fmt| 0         | cs id - 64    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Chunk stream IDs 64-65599 can be encoded in the 3-byte version of
 * this field. ID is computed as ((the third byte)*256 + the second byte
 * + 64).
 * 0                   1                   2
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |fmt| 1         | cs id - 64                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * cs id: 6 bits
 * 	This field contains the chunk stream ID, for values from 2-63.
 * 	Values 0 and 1 are used to indicate the 2- or 3-byte versions of
 * 	this field.
 * fmt: 2 bits
 * 	This field identifies one of four format used by the .chunk message
 * 	header..The .chunk message header. for each of the chunk types is
 * 	explained in the next section.
 * cs id - 64: 8 or 16 bits
 * 	This field contains the chunk stream ID minus 64. For example, ID
 * 	365 would be represented by a 1 in cs id, and a 16-bit 301 here.
 * 	Chunk stream IDs with values 64-319 could be represented by both 2-
 * 	byte version and 3-byte version of this field.
 *
 ************************************************************/
class RTMPChunkBasicHeader : public RTMPFormat<3>
{
public:
	DWORD 	GetFmt() 	 { return data[0]>>6; }
	void	SetFmt(BYTE fmt) { data[0] = (fmt<<6) | (data[0]&0x3F); }	

	DWORD	GetStreamId() 
	{
		switch(size)
		{
			case 1:
				return data[0] & 0x3F;
			case 2:
				return data[1] + 64;
			case 3:
				return (((DWORD)data[1])<<8) + data[2] + 64;
		}
		return (DWORD)-1;
	}
	void	SetStreamId(DWORD csid)
	{
		if (csid<64)
		{
			//1 byte header
			size = 1;
			data[0] = (data[0]&0xC0) | csid;
		} else if (csid<320) {
			//2 byte header
			size = 2;
			data[0] &= 0xC0;
			data[1] = (csid-64);
		} else {
			//3 byte header
			size = 3;
			data[0] = (data[0]&0xC0) | 1;
			data[1] = (csid-64) & 0xFF;
			data[2] = (csid-64) >> 8;
		}
	}

	virtual void PreParse(BYTE *buffer,DWORD bufferSize)
	{
		//Get the first byte of the header
		if (len==0)
		{
			//Get header length
			switch(buffer[0] & 0x3F)
			{
				case 0:
					//2 byte header
					size = 2;
					break;
				case 1:
					//2 byte header
					size = 3;
					break;
				default:
					//1 byte header
					size = 1;
					break;
			}
		}
	}

	virtual void Dump()
	{
		Debug("RTMPChunkBasicHeader [fmt:%d,id:%d,size:%d]\n",GetFmt(),GetStreamId(),GetSize());
	}
};

/***********************************************************
 * Chunk Type 0
 * Chunks of Type 0 are 11 bytes long. This type MUST be used at the
 * start of a chunk stream, and whenever the stream timestamp goes
 * backward (e.g., because of a backward seek).
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | timestamp                                     |message length |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | message length (cont)         |message type id| msg stream id |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | message stream id (cont)                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * timestamp: 3 bytes
 * 	For a type-0 chunk, the absolute timestamp of the message is sent
 * 	here. If the timestamp is greater than or equal to 16777215
 * 	(hexadecimal 0x00ffffff), this value MUST be 16777215, and the
 * 	.extended timestamp header. MUST be present. Otherwise, this value
 * 	SHOULD be the entire timestamp.
 *
 * msg stream id: 4 byes
 * 	For some strange reason it is not in nework byte order, it is LE.
 *
 ******************************************************************/

class RTMPExtendedTimestamp : public RTMPFormat<4>
{
public:
	DWORD 	GetTimestamp() 		{ return get4(data,0); }
        void    SetTimestamp(DWORD timestamp) 		{ set4(data,0,timestamp); }
};

class RTMPChunkType0 : public RTMPFormat<11>
{
public:
	DWORD 	GetTimestamp() 		{ return get3(data,0); }
	DWORD 	GetMessageLength() 	{ return get3(data,3); }
	BYTE 	GetMessageTypeId()	{ return get1(data,6); }
	DWORD 	GetMessageStreamId() 	{ return *(DWORD*)(data+7); }

        void    SetTimestamp(DWORD timestamp) 		{ set3(data,0,timestamp); }
	void	SetMessageLength(DWORD length) 		{ set3(data,3,length); }
	void	SetMessageTypeId(BYTE typeId)		{ set1(data,6,typeId); }
	void	SetMessageStreamId(DWORD streamId)	{ memcpy(data+7,(BYTE*)&streamId,4); }

	virtual void Dump()
	{
		Debug("RTMPChunkType0 [timestamp:%d,lenght:%d,type:%d,streamId:%d]\n",GetTimestamp(),GetMessageLength(),GetMessageTypeId(),GetMessageStreamId());
	}

};


/**************************************************************
 * Type 1
 * Chunks of Type 1 are 7 bytes long. The message stream ID is not
 * included; this chunk takes the same stream ID as the preceding chunk.
 * Streams with variable-sized messages (for example, many video
 * formats) SHOULD use this format for the first chunk of each new
 * message after the first.
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | timestamp delta                               |message length |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | message length (cont)         |message type id|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *************************************************************/
class RTMPChunkType1 : public RTMPFormat<7>
{
public:
	DWORD 	GetTimestampDelta()	{ return get3(data,0); }
	DWORD 	GetMessageLength() 	{ return get3(data,3); }
	BYTE 	GetMessageTypeId()	{ return get1(data,6); }

        void    SetTimestampDelta(DWORD timestamp) 	{ set3(data,0,timestamp); }
	void	SetMessageLength(DWORD length) 		{ set3(data,3,length); }
	void	SetMessageTypeId(BYTE typeId)		{ set1(data,6,typeId); }

	virtual void Dump()
	{
		Debug("RTMPChunkType1 [timestampDelta:%d,lenght:%d,type:%d]\n",GetTimestampDelta(),GetMessageLength(),GetMessageTypeId());
	}
};

/*************************************************************
 * Type 2
 * Chunks of Type 2 are 3 bytes long. Neither the stream ID nor the
 * message length is included; this chunk has the same stream ID and
 * message length as the preceding chunk. Streams with constant-sized
 * messages (for example, some audio and data formats) SHOULD use this
 * format for the first chunk of each message after the first.
 * 0                   1                   2
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | timestamp delta                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * ***********************************************************/
class RTMPChunkType2 : public RTMPFormat<3>
{
public:
	DWORD 	GetTimestampDelta()	{ return get3(data,0); }
        void    SetTimestampDelta(DWORD timestamp) 	{ set3(data,0,timestamp); }
	virtual void Dump()
	{
		Debug("RTMPChunkType2 [timestampDelta:%d]\n",GetTimestampDelta());
	}
};

/*************************************************************
 * Type 3
 * Chunks of Type 3 have no header. Stream ID, message length and
 * timestamp delta are not present; chunks of this type take values from
 * the preceding chunk.
 ************************************************************/

/************************************************************
 * C0 and S0 Format
 * The C0 and S0 packets are a single octet, treated as a single 8-bit
 * integer field:
 * 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+
 * |    version    |
 * +-+-+-+-+-+-+-+-+
 * Following are the fields in the C0/S0 packets:
 * Version: 8 bits
 * 	In C0, this field identifies the RTMP version requested by the
 * 	client. In S0, this field identifies the RTMP version selected by
 * 	the server. The version defined by this specification is 3. Values
 * 	0-2 are deprecated values used by earlier proprietary products; 4-
 * 	31 are reserved for future implementations; and 32-255 are not
 * 	allowed (to allow distinguishing RTMP from text-based protocols,
 * 	which always start with a printable character). A server that does
 * 	not recognize the client's requested version SHOULD respond with 3.
 * 	The client MAY choose to degrade to version 3, or to abandon the
 * 	handshake.
 **************************************************************************/
class RTMPHandshake0 : public RTMPFormat<1>
{
public:
	DWORD 	GetRTMPVersion() { return get1(data,0); }
	void	SetRTMPVersion(BYTE version) { set1(data,0,version); }
};

/*****************************************************************
 * C1 and S1 Format
 * The C1 and S1 packets are 1536 octets long, consisting of the
 * following fields:
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     time (4 bytes)                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     zero (4 bytes)                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     random bytes                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     random bytes                              |
 * |                     (cont)                                    |
 * |                     ....                                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * Time: 4 bytes
 * 	This field contains a timestamp, which SHOULD be used as the epoch
 * 	for all future chunks sent from this endpoint. This may be 0, or
 * 	some arbitrary value. To synchronize multiple chunkstreams, the
 * 	endpoint may wish to send the current value of the other
 * 	chunkstream's timestamp.
 * Zero: 4 bytes
 * 	This field MUST be all 0s.
 * Random data: 1528 bytes
 * 	This field can contain any arbitrary values. Since each endpoint
 * 	has to distinguish between the response to the handshake it has
 * 	initiated and the handshake initiated by its peer,this data SHOULD
 * 	send something sufficiently random. But there is no need for
 * 	cryptographically-secure randomness, or even dynamic values.
 *
 *  Zero is not 0 any more for flash player >9
 *  	FP10 - byte0=128
 *  	FP9 - 
 *  When dealying with clients > flash player 9 the server should return the version 
 ***********************************************************************/
class RTMPHandshake01 : public RTMPFormat<1537>
{
public:
	DWORD 	GetRTMPVersion() { return get1(data,0); }
	void	SetRTMPVersion(BYTE version) { set1(data,0,version); }
	DWORD 	GetTime() 		{ return get4(data,1); } 
        void    SetTime(DWORD time) 	{ set4(data,1,time); }

        void    SetVersion(BYTE b0,BYTE b1,BYTE b2,BYTE b3)
	{
		set1(data,5,b0);
		set1(data,6,b1);
		set1(data,7,b2);
		set1(data,8,b3);
	}
	BYTE*	GetVersion()	{ return data+5; }
	BYTE* 	GetRandom() 	{ return data+9; }
	DWORD 	GetRandomSize()	{ return 1528;   }
	void  	SetRandom(BYTE* buffer,DWORD size) { if (size==1528) memcpy(data+9,buffer,size); }	
};

class RTMPHandshake1 : public RTMPFormat<1536>
{
public:
	DWORD 	GetTime() 		{ return get4(data,0); } 
        void    SetTime(DWORD time) 	{ set4(data,0,time); }

        void    SetVersion(BYTE b0,BYTE b1,BYTE b2,BYTE b3)
	{
		set1(data,4,b0);
		set1(data,5,b1);
		set1(data,6,b2);
		set1(data,7,b3);
	}
	BYTE*	GetVersion()	{ return data+4; }
	BYTE* 	GetRandom() 	{ return data+8; }
	DWORD 	GetRandomSize()	{ return 1528;   }
	void  	SetRandom(BYTE* buffer,DWORD size) { if (size==1528) memcpy(data+8,buffer,size); }	

};

/*****************************************************************
 * C2 and S2 Format
 * The C2 and S2 packets are 1536 octets long, and nearly an echo of S1
 * and C1 (respectively), consisting of the following fields:
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     time (4 bytes)                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     time2(4 bytes)                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     random echo bytes                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     random echo bytes                         |
 * |                     (cont)                                    |
 * |                     ....                                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * Time: 4 bytes
 * 	This field MUST contain the timestamp sent by the peer in S1 (for
 * 	C2) or C1 (for S2).
 * Time2: 4 bytes
 * 	This field MUST contain the timestamp at which the previous
 * 	packet(s1 0r c1) sent by the peer was read.
 * Random echo: 1528 bytes
 * 	This field MUST contain the random data field sent by the peer in
 * 	S1 (for C2) or S2 (for C1).
 * Either peer can use the time and time2 fields together with the
 * current timestamp as a quick estimate of the bandwidth and/or
 * latency of the connection, but this is unlikely to be useful.
 ***********************************************************************/
class RTMPHandshake2 : public RTMPFormat<1536>
{
public:
	DWORD 	GetTime() 	{ return get4(data,0); }
	DWORD 	GetTime2() 	{ return get4(data,4); }
        void    SetTime(DWORD time) 	{ set4(data,0,time); }
        void    SetTime2(DWORD time2) 	{ set4(data,4,time2); }

	BYTE* 	GetRandom() 	{ return data+8; }
	DWORD 	GetRandomSize()	{ return 1528;   }
	void  	SetRandom(BYTE* buffer,DWORD size) { if (size==1528) memcpy(data+8,buffer,size); }	
};

/**********************************************************************
 * Set Chunk Size (1)
 * Protocol control message 1, Set Chunk Size, is used to notify the
 * peer a new maximum chunk size to use.
 * The value of the chunk size is carried as 4-byte message payload. A
 * default value exists for chunk size, but if the sender wants to
 * change this value it notifies the peer about it through this
 * protocol message. For example, a client wants to send 131 bytes of
 * data and the chunk size is at its default value of 128. So every
 * message from the client gets split into two chunks. The client can
 * choose to change the chunk size to 131 so that every message get
 * split into two chunks. The client MUST send this protocol message to
 * the server to notify that the chunk size is set to 131 bytes.
 * The maximum chunk size can be 65536 bytes. Chunk size is maintained
 * independently for server to client communication and client to server
 * communication.
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 chunk size (4 bytes)                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * chunk size: 32 bits
 * 	This field holds the new chunk size, which will be used for all
 * 	future chunks sent by this chunk stream.
 ******************************************************************/
class RTMPSetChunkSize : public RTMPFormat<4>
{
public:
	static RTMPSetChunkSize* Create(DWORD size)
	{
		RTMPSetChunkSize* obj = new RTMPSetChunkSize();
		obj->SetChunkSize(size);
		return obj;
	}
	DWORD 	GetChunkSize() 			{ return get4(data,0); }
        void    SetChunkSize(DWORD size) 	{ set4(data,0,size); }
};

/*********************************************************************
 * Abort Message (2)
 * Protocol control message 2, Abort Message, is used to notify the peer
 * if it is waiting for chunks to complete a message, then to discard
 TMPAcknowledgementueived message over a chunk stream and abort
 * processing of that message. The peer receives the chunk stream ID of
 * the message to be discarded as payload of this protocol message. This
 * message is sent when the sender has sent part of a message, but wants
 * to tell the receiver that the rest of the message will not be sent.
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   chunk stream id (4 bytes)                   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * chunk stream ID: 32 bits
 * 	This field holds the chunk stream ID, whose message is to be
 * 	discarded.
 **********************************************************************/
class RTMPAbortMessage : public RTMPFormat<4>
{
public:
	static RTMPAbortMessage* Create(DWORD id)
	{
		RTMPAbortMessage* obj = new RTMPAbortMessage();
		obj->SetChunkStreamId(id);
		return obj;
	}
	DWORD 	GetChunkStreamId() 		{ return get4(data,0); }
        void    SetChunkStreamId(DWORD id) 	{ set4(data,0,id); }
};

/**********************************************************************
 * The client or the server sends the acknowledgment to the peer after
 * receiving bytes equal to the window size. The window size is the
 * maximum number of bytes that the sender sends without receiving
 * acknowledgment from the receiver. The server sends the window size to
 * the client after application connects. This message specifies the
 * sequence number, which is the number of the bytes received so far.
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | sequence number (4 bytes) |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * sequence number: 32 bits
 * 	This field holds the number of bytes received so far.
 ***********************************************************************/
class RTMPAcknowledgement : public RTMPFormat<4>
{
public:
	static RTMPAcknowledgement* Create(DWORD seq)
	{
		RTMPAcknowledgement* obj = new RTMPAcknowledgement();
		obj->SetSeqNumber(seq);
		return obj;
	}

	DWORD 	GetSeNumber() 		{ return get4(data,0); }
        void    SetSeqNumber(DWORD seq)	{ set4(data,0,seq); }
};

/***************************************************************************
 * User Control Message (4)
 * The client or the server sends this message to notify the peer about
 * the user control events. This message carries Event type and Event
 * data.
 *
 * +------------------------------+-------------------------
 * | Event Type ( 2- bytes ) | Event Data
 * +------------------------------+-------------------------
 *
 * The first 2 bytes of the message data are used to identify the Event
 * type. Event type is followed by Event data. Size of Event data field
 * is variable.
 *
 * The following user control event types are supported:
 * Stream Begin 	 The server sends this event to notify the client 
 *  (=0) 		 that a stream has become functional and can be 
 *  			 used for communication. By default, this event 
 *  			 is sent on ID 0 after the application connect 
 *  			 command is successfully received from the 
 *  			 client. The event data is 4-byte and represents 
 *  			 the stream ID of the stream that became 
 *  			 functional. 
 *
 *  Stream EOF 	 	 The server sends this event to notify the client 
 *  (=1) 		 that the playback of data is over as requested 
 *  			 on this stream. No more data is sent without 
 *  			 issuing additional commands. The client discards 
 *  			 the messages received for the stream. The 
 *  			 4 bytes of event data represent the ID of the 
 *  			 stream on which playback has ended. 
 *
 *  StreamDry 		 The server sends this event to notify the client 
 *  (=2) 		 that there is no more data on the stream. If the 
 *  			 server does not detect any message for a time 
 *  			 period, it can notify the subscribed clients 
 *  			 that the stream is dry. The 4 bytes of event 
 *  			 data represent the stream ID of the dry stream. 
 *
 *  SetBuffer 		 The client sends this event to inform the server 
 *  Length (=3) 	 of the buffer size (in milliseconds) that is 
 *  			 used to buffer any data coming over a stream. 
 *  			 This event is sent before the server starts 
 *  			 processing the stream. The first 4 bytes of the 
 *  			 event data represent the stream ID and the next 
 *  			 4 bytes represent the buffer length, in 
 *  			 milliseconds. 
 *
 *  StreamIs 		 The server sends this event to notify the client 
 *  Recorded (=4) 	 that the stream is a recorded stream. The 
 *  			 4 bytes event data represent the stream ID of 
 *  			 the recorded stream. 
 *
 *  PingRequest 	 The server sends this event to test whether the 
 *  (=6) 		 client is reachable. Event data is a 4-byte 
 *  			 timestamp, representing the local server time 
 *  			 when the server dispatched the command. The 
 *  			 client responds with kMsgPingResponse on 
 *  			 receiving kMsgPingRequest. 
 *
 *  PingResponse 	 The client sends this event to the server in 
 *  (=7) 		 response to the ping request. The event data is 
 *  			 a 4-byte timestamp, which was received with the 
 *  			 kMsgPingRequest request. 
 ***************************************************************************/
class RTMPUserControlMessage : public RTMPFormat<10>
{
public:
	enum EventType
	{
		StreamBegin = 0,
		StreamEOF = 1,
		StreamDry = 2,
		SetBufferLength = 3,
		StreamIsRecorded = 4,
		PingRequest = 6,
		PingResponse = 7
	};
private:
	RTMPUserControlMessage(EventType type,DWORD d) : RTMPFormat<10>()
	{
		this->size = 6;
		SetEventType(type);
		SetEventData(d);
		
	}

	RTMPUserControlMessage(EventType type,DWORD d,DWORD d2) : RTMPFormat<10>()
	{
		this->size = 10;
		SetEventType(type);
		SetEventData(d);
		SetEventData(d2);
	}
		
public:
	RTMPUserControlMessage() : RTMPFormat<10>()
	{
	}

	static RTMPUserControlMessage* CreateStreamBegin(DWORD messageStreamId)
		{ return new RTMPUserControlMessage(StreamBegin,messageStreamId); }

	static RTMPUserControlMessage* CreateStreamEOF(DWORD messageStreamId)
		{ return new RTMPUserControlMessage(StreamEOF,messageStreamId); }

	static RTMPUserControlMessage* CreateSetBufferLength(DWORD messageStreamId,DWORD milis)
		{ return new RTMPUserControlMessage(SetBufferLength,messageStreamId,milis); }

	static RTMPUserControlMessage* CreateStreamIsRecorded(DWORD messageStreamId)
		{ return new RTMPUserControlMessage(StreamIsRecorded,messageStreamId); }

	static RTMPUserControlMessage* CreatePingRequest(DWORD seq)
		{ return new RTMPUserControlMessage(PingRequest,seq); }

	static RTMPUserControlMessage* CreatePingResponse(DWORD seq)
		{ return new RTMPUserControlMessage(PingResponse,seq); }
	

	EventType 	GetEventType() 			{ return (EventType)get2(data,0); }
        void    	SetEventType(EventType type)	{ set2(data,0,(WORD)type); }

	DWORD 	GetEventData() 		{ return get4(data,2); }
        void    SetEventData(DWORD d)	{ set4(data,2,d); }
	DWORD 	GetEventData2() 	{ return get4(data,6); }
        void    SetEventData2(DWORD d)	{ set4(data,6,d); }

	virtual void PreParse(BYTE *buffer,DWORD bufferSize)
	{
		//check if have enought data to get the type 
		if ((len==0) && (bufferSize>1))
		{ 
			//Check type is Set buffer syze
			if (buffer[1]==3)
				//Long
				size = 10;
			else
				//Short
				size = 6;
		} else if (len==1) {
			//Check type is Set buffer syze
			if (buffer[0]==3)
				//Long
				size = 10;
			else
				//Short
				size = 6;
		}
	}
};


/********************************************************************************
 * Window Acknowledgement Size (5)
 * The client or the server sends this message to inform the peer which
 * window size to use when sending acknowledgment. For example, a server
 * expects acknowledgment from the client every time the server sends
 * bytes equivalent to the window size. The server updates the client
 * about its window size after successful processing of a connect
 * request from the client.
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   Acknowledgement Window size (4 bytes)       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 ***************************************************************************************/
class RTMPWindowAcknowledgementSize : public RTMPFormat<4>
{
public:
	static RTMPWindowAcknowledgementSize* Create(DWORD windowSize)
	{
		RTMPWindowAcknowledgementSize* obj = new RTMPWindowAcknowledgementSize();
		obj->SetWindowSize(windowSize);
		return obj;
	}
	DWORD 	GetWindowSize() 		{ return get4(data,0); }
        void    SetWindowSize(DWORD windowSize)	{ set4(data,0,windowSize); }
};

/********************************************************************************
 * Set Peer Bandwidth (6)
 * The client or the server sends this message to update the output
 * bandwidth of the peer. The output bandwidth value is the same as the
 * window size for the peer. The peer sends .Window Acknowledgement
 * Size. back if its present window size is different from the one
 * received in the message.
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   Acknowledgement Window size                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Limit type    |
 * +-+-+-+-+-+-+-+-+
 * The sender can mark this message hard (0), soft (1), or dynamic (2)
 * using the Limit type field. In a hard (0) request, the peer must send
 * the data in the provided bandwidth. In a soft (1) request, the
 * bandwidth is at the discretion of the peer and the sender can limit
 * the bandwidth. In a dynamic (2) request, the bandwidth can be hard or
 * soft.
 ******************************************************************************/
class RTMPSetPeerBandWidth : public RTMPFormat<5>
{
public:
	static RTMPSetPeerBandWidth* Create(DWORD size,BYTE type)
	{
		RTMPSetPeerBandWidth* obj = new RTMPSetPeerBandWidth();
		obj->SetWindowSize(size);
		obj->SetType(type);
		return obj;
	}
	DWORD 	GetWindowSize() 		{ return get4(data,0); }
        void    SetWindowSize(DWORD size)	{ set4(data,0,size); }
	BYTE 	GetType() 		{ return get1(data,4); }
	void 	SetType(BYTE type) 	{ set1(data,4,type); }
};

#endif
