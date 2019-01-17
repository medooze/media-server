/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPPayloadFeedback.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:04
 */

#include "rtp/RTCPPayloadFeedback.h"
#include "rtp/RTCPCommonHeader.h"

RTCPPayloadFeedback::RTCPPayloadFeedback() : 
	RTCPPacket(RTCPPacket::PayloadFeedback)
{

}

RTCPPayloadFeedback::RTCPPayloadFeedback(RTCPPayloadFeedback::FeedbackType type,DWORD senderSSRC,DWORD mediaSSRC) : 
	RTCPPacket(RTCPPacket::PayloadFeedback),
	feedbackType(type),
	senderSSRC(senderSSRC),
	mediaSSRC(mediaSSRC)
{
	
}


void RTCPPayloadFeedback::Dump()
{
	Debug("\t[RTCPPacket PayloadFeedback %s sender:%u media:%u]\n",TypeToString(feedbackType),senderSSRC,mediaSSRC);
	for (DWORD i=0;i<fields.size();i++)
	{
		//Check type
		switch(feedbackType)
		{
			case RTCPPayloadFeedback::PictureLossIndication:
			case RTCPPayloadFeedback::FullIntraRequest:
			case RTCPPayloadFeedback::SliceLossIndication:
			case RTCPPayloadFeedback::ReferencePictureSelectionIndication:
			case RTCPPayloadFeedback::TemporalSpatialTradeOffRequest:
			case RTCPPayloadFeedback::TemporalSpatialTradeOffNotification:
			case RTCPPayloadFeedback::VideoBackChannelMessage:
				break;
			case RTCPPayloadFeedback:: ApplicationLayerFeeedbackMessage:
			{
				//Get field
				auto field = std::static_pointer_cast<ApplicationLayerFeeedbackField>(fields[i]);
				//Get size and payload
				DWORD len		= field->GetLength();
				const BYTE* payload	= field->GetPayload();
				//Dump it
				//::Dump(payload,len);
				//Check if it is a REMB
				if (len>8 && payload[0]=='R' && payload[1]=='E' && payload[2]=='M' && payload[3]=='B')
				{
					//Get num of ssrcs
					BYTE num = payload[4];
					//GEt exponent
					BYTE exp = payload[5] >> 2;
					DWORD mantisa = payload[5] & 0x03;
					mantisa = mantisa << 8 | payload[6];
					mantisa = mantisa << 8 | payload[7];
					//Get bitrate
					DWORD bitrate = mantisa << exp;
					//Log
					Debug("\t[REMB bitrate=%d exp=%d mantisa=%d num=%d/]\n",bitrate,exp,mantisa,num);
					//For each
					for (DWORD i=0;i<num;++i)
					{
						//Check length
						if (len<8+4*i+4)
							//wrong format
							break;
						//Log
						Debug("\t[ssrc=%u/]\n",get4(payload,8+4*i));
					}
					//Log
					Debug("\t[/REMB]\n");
					
				}
				
				break;
			}
		}
	}
	Debug("\t[/RTCPPacket PayloadFeedback %s]\n",TypeToString(feedbackType));
}
DWORD RTCPPayloadFeedback::GetSize()
{
	DWORD len = 8+RTCPCommonHeader::GetSize();
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}

DWORD RTCPPayloadFeedback::Parse(const BYTE* data,DWORD size)
{
//Get header
	RTCPCommonHeader header;
		
	//Parse header
	DWORD len = header.Parse(data,size);
	
	//IF error
	if (!len)
		return 0;
		
	//Get packet size
	DWORD packetSize = header.length;
	
	//Check size
	if (size<packetSize)
		//Exit
		return 0;
	
	//Get subtype
	feedbackType = (FeedbackType)header.count;

	//Get ssrcs
	senderSSRC = get4(data,len);
	mediaSSRC = get4(data,len+4);
	//skip fields
	len += 8;
	//While we have more
	while (len<packetSize)
	{
		Field::shared field = nullptr;
		//Depending on the type
		switch(feedbackType)
		{
			case PictureLossIndication:
				return Error("PictureLossIndication with body\n");
			case SliceLossIndication:
				field = std::make_shared<SliceLossIndicationField>();
				break;
			case ReferencePictureSelectionIndication:
				field = std::make_shared<ReferencePictureSelectionField>();
				break;
			case FullIntraRequest:
				field = std::make_shared<FullIntraRequestField>();
				break;
			case TemporalSpatialTradeOffRequest:
			case TemporalSpatialTradeOffNotification:
				field = std::make_shared<TemporalSpatialTradeOffField>();
				break;
			case VideoBackChannelMessage:
				field = std::make_shared<VideoBackChannelMessageField>();
				break;
			case ApplicationLayerFeeedbackMessage:
				field = std::make_shared<ApplicationLayerFeeedbackField>();
				break;
			default:
				return Error("Unknown RTCPPayloadFeedback type [%d]\n",header.count);
		}
		//Parse field
		DWORD parsed = field->Parse(data+len,packetSize-len);
		//If not parsed
		if (!parsed)
			//Error
			return 0;
		//Add field
		fields.push_back(field);
		//Skip
		len += parsed;
	}
	//Return consumed len
	return len;
}

DWORD RTCPPayloadFeedback::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPPayloadFeedback invalid size\n");

	//RTCP common header
	RTCPCommonHeader header;
	//Set values
	header.count	  = feedbackType;
	header.packetType = GetType();
	header.padding	  = 0;
	header.length	  = packetSize;
	//Serialize
	DWORD len = header.Serialize(data,size);
	
	//Set ssrcs
	set4(data,len,senderSSRC);
	set4(data,len+4,mediaSSRC);
	//Inclrease len
	len += 8;
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//Serialize it
		len+=(*it)->Serialize(data+len,size-len);
	//Retrun writed data len
	return len;
}
