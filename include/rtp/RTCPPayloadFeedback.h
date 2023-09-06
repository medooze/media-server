/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPPayloadFeedback.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:04
 */

#ifndef RTCPPAYLOADFEEDBACK_H
#define RTCPPAYLOADFEEDBACK_H

#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"
#include <vector>
#include <list>
#include <memory>

class RTCPPayloadFeedback : public RTCPPacket
{
public:
	using shared = std::shared_ptr<RTCPPayloadFeedback>;
	
	enum FeedbackType {
		UNKNOWN=0,
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
			case UNKNOWN:
				return "Unkown";
		}
		return "Unknown";
	}

	
public:
	struct Field
	{
		using shared = std::shared_ptr<Field>;
		virtual ~Field(){};
		virtual DWORD GetSize() = 0;
		virtual DWORD Parse(const BYTE* data,DWORD size) = 0;
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
		virtual DWORD Parse(const BYTE* data,DWORD size)
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
			padding = 0;
			type = 0;
			payload = NULL;
			length = 0;
		}

		~ReferencePictureSelectionField()
		{
			if (payload) free(payload);
		}

		virtual DWORD GetSize() { return 2+length+padding;}
		virtual DWORD Parse(const BYTE* data,DWORD size)
		{
			if (size<2) return 0;
			//Get values
			padding = data[0];
			type = data[1];
			if (size<padding+2u) return 0;
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
		virtual DWORD Parse(const BYTE* data,DWORD size)
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
		virtual DWORD Parse(const BYTE* data,DWORD size)
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
			ssrc = 0;
			seq = 0;
			type = 0;
			payload = 0;
			length = 0;
		}
		~VideoBackChannelMessageField()
		{
			if (payload) free(payload);
		}
		virtual DWORD GetSize() { return 8+length;}
		virtual DWORD Parse(const BYTE* data,DWORD size)
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
		virtual DWORD Parse(const BYTE* data,DWORD size)
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

		static ApplicationLayerFeeedbackField::shared CreateReceiverEstimatedMaxBitrate(std::list<DWORD> ssrcs,DWORD bitrate)
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
			for(BYTE i=0;i<32;i++)
			{
				//If bitrate is less than
				if(bitrate <= (0x003FFFFu << i))
				{
					//That's the exp
					bitrateExp = i;
					break;
				}
			}
			//Get mantisa
			DWORD bitrateMantissa = (bitrate >> bitrateExp);
			//Create field
			auto field = std::make_shared<ApplicationLayerFeeedbackField>();
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
	RTCPPayloadFeedback(FeedbackType type,DWORD senderSSRC,DWORD mediaSSRC);
	virtual ~RTCPPayloadFeedback() = default;

	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	void SetSenderSSRC(DWORD ssrc)			{ senderSSRC = ssrc;		}
	void SetMediaSSRC(DWORD ssrc)			{ mediaSSRC = ssrc;		}
	void SetFeedBackTtype(FeedbackType type)	{ feedbackType = type;		}
	void AddField(const Field::shared& field)	{ fields.push_back(field);	}
	
	template<typename Type>
	void AddField(const std::shared_ptr<Type>& field) { AddField(std::static_pointer_cast<Field>(field));	}
	
	template<typename Type,class ...Args>
	std::shared_ptr<Type> CreateField(Args... args)
	{
		auto field =  std::make_shared<Type>(Type{ std::forward<Args>(args)... });
		AddField(std::static_pointer_cast<Field>(field));
		return field;
	}

	DWORD		GetMediaSSRC() const	{ return mediaSSRC;		}
	DWORD		GetSenderSSRC() const	{ return senderSSRC;		}
	FeedbackType	GetFeedbackType() const	{ return feedbackType;		}
	DWORD		GetFieldCount() const	{ return fields.size();		}
	Field::shared	GetField(BYTE i) const	{ return fields[i];		}
	
	template<typename Type>
	const std::shared_ptr<Type>	GetField(BYTE i)	const { return std::static_pointer_cast<Type>(fields[i]); }
private:
	typedef std::vector<Field::shared> Fields;
private:
	FeedbackType feedbackType = UNKNOWN;
	DWORD senderSSRC = 0;
	DWORD mediaSSRC = 0;
	Fields fields;
};


#endif /* RTCPPAYLOADFEEDBACK_H */

