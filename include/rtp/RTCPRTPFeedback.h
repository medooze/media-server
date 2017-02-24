/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPRTPFeedback.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:03
 */

#ifndef RTCPRTPFEEDBACK_H
#define RTCPRTPFEEDBACK_H
#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"
#include <vector>
#include <list>
#include <map>

class RTCPRTPFeedback : public RTCPPacket
{
public:
	enum FeedbackType {
		NACK = 1,
		TempMaxMediaStreamBitrateRequest = 3,
		TempMaxMediaStreamBitrateNotification = 4,
		TransportWideFeedbackMessage = 15
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
			case TransportWideFeedbackMessage:
				return "TransportWideFeedbackMessage";
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
		virtual ~NACKField(){}
		
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
		virtual ~TempMaxMediaStreamBitrateField(){}
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
			for(BYTE i=0;i<32;i++)
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
	
	struct TransportWideFeedbackMessageField : public Field
	{
		enum PacketStatus
		{
			NotReceived = 0,
			SmallDelta = 1,
			LargeOrNegativeDelta = 2,
			Reserved = 3
		};
		/*
		        0                   1                   2                   3
			0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |V=2|P|  FMT=15 |    PT=205     |           length              |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |                     SSRC of packet sender                     |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |                      SSRC of media source                     |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |      base sequence number     |      packet status count      |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |                 reference time                | fb pkt. count |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |          packet chunk         |         packet chunk          |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       .                                                               .
		       .                                                               .
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |         packet chunk          |  recv delta   |  recv delta   |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       .                                                               .
		       .                                                               .
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		       |           recv delta          |  recv delta   | zero padding  |
		       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


		 */
		TransportWideFeedbackMessageField()
		{
			
		}
		
		TransportWideFeedbackMessageField(DWORD feedbackPacketCount)
		{
			this->feedbackPacketCount = feedbackPacketCount;
		}
		virtual ~TransportWideFeedbackMessageField(){}
		
		//Pair<seqnum,us> -> us = 0, not received
		typedef std::map<WORD,QWORD> Packets;
		
		virtual DWORD GetSize();
		virtual DWORD Parse(BYTE* data,DWORD size);
		virtual DWORD Serialize(BYTE* data,DWORD size);
		
		BYTE feedbackPacketCount;
		Packets packets;
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


#endif /* RTCPRTPFEEDBACK_H */

