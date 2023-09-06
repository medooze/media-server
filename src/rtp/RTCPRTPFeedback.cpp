/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPRTPFeedback.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 12:03
 */

#include "rtp/RTCPRTPFeedback.h"
#include "rtp/RTCPCommonHeader.h"


RTCPRTPFeedback::RTCPRTPFeedback() : RTCPPacket(RTCPPacket::RTPFeedback)
{
}
	
RTCPRTPFeedback::RTCPRTPFeedback(RTCPRTPFeedback::FeedbackType type,DWORD senderSSRC,DWORD mediaSSRC) :  
	RTCPPacket(RTCPPacket::RTPFeedback),
	feedbackType(type),
	senderSSRC(senderSSRC),
	mediaSSRC(mediaSSRC)
{
}


DWORD RTCPRTPFeedback::GetSize() 
{
	DWORD len = 8 + RTCPCommonHeader::GetSize();
	//For each field
	for (Fields::iterator it=fields.begin();it!=fields.end();++it)
		//add size
		len += (*it)->GetSize();
	return len;
}

DWORD RTCPRTPFeedback::Parse(const BYTE* data,DWORD size)
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
			case NACK:
				field = std::make_shared<NACKField>();
				break;
			case TempMaxMediaStreamBitrateRequest:
			case TempMaxMediaStreamBitrateNotification:
				field = std::make_shared<TempMaxMediaStreamBitrateField>();
				break;
			case TransportWideFeedbackMessage:
				field = std::make_shared<TransportWideFeedbackMessageField>();
				break;
			default:
				return Error("Unknown RTCPRTPFeedback type [%d]\n",header.count);
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
	return len+12;
}

void RTCPRTPFeedback::Dump()
{
	Debug("\t[RTCPPacket Feedback %s sender:%u media:%u]\n",TypeToString(feedbackType),senderSSRC,mediaSSRC);
	for (DWORD i=0;i<fields.size();i++)
		//Dump it
		fields[i]->Dump();
	Debug("\t[/RTCPPacket Feedback %s]\n",TypeToString(feedbackType));
}

DWORD RTCPRTPFeedback::Serialize(BYTE* data,DWORD size)
{
	//Get packet size
	DWORD packetSize = GetSize();
	//Check size
	if (size<packetSize)
		//error
		return Error("Serialize RTCPRTPFeedback invalid size [size:%d,packetSize:%d]\n",size,packetSize);

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
	
	if (len!=packetSize)
		return Error("-Incorrect rtcp packet size [size:%d,actual:%d]\n",packetSize,len);
	
	//Retrun writed data len
	return len;
}


DWORD RTCPRTPFeedback::TransportWideFeedbackMessageField::GetSize() const
{
	//If we have no packets
	if (packets.size()==0)
		return 0;
	
	//Initial time in us
	QWORD time = 0;
	
	//Calculate temporal info
	bool firstReceived	= false;
	QWORD referenceTime	= 0;
	
	//Store delta array
	std::list<int> deltas;
	std::list<PacketStatus> statuses;
	PacketStatus lastStatus = PacketStatus::Reserved;
	PacketStatus maxStatus = PacketStatus::NotReceived;
	bool allsame = true;
	
	//Header
	DWORD len = 8;
	
	//For each packet 
	for (Packets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
	{
		PacketStatus status = PacketStatus::NotReceived;
		
		//If got packet
		if (it->second)
		{
			int delta = 0;
			//If first received
			if (!firstReceived)
			{
				//Got it 
				firstReceived = true;
				//Set it as 3 bytes signed integer
				referenceTime = (it->second/64000) & 0x7FFFFF;
				//Get initial time
				time = referenceTime * 64000;
			}
					
			//Get delta
			if (it->second>time)
				delta = (it->second-time)/250;
			else
				delta = -(int)((time-it->second)/250);
			//If it is negative or to big
			if (delta<0 || delta> 127)
				//Big one
				status = PacketStatus::LargeOrNegativeDelta;
			else
				//Small
				status = PacketStatus::SmallDelta;
			//Store delta
			deltas.push_back(delta);
			//Set next time
			time = time + delta*250;
		}
		
		//Check if all previoues ones were equal and this one the firt different
		if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
		{
			//Anything bigger is worth a run
			if (statuses.size()>7)
			{
				//One chunk
				len+=2;
				//Remove all statuses
				statuses.clear();
				//REset
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
			} else {
				//Not same
				allsame = false;
			}
		}
		
		//Push back statuses, it will be handled later
		statuses.push_back(status);
		
		//If it is bigger
		if (status>maxStatus)
			//Store it
			maxStatus = status;
		//Store las status
		lastStatus = status;

		//If they are different already
		if (!allsame)
		{
			//Check 
			if (maxStatus==PacketStatus::LargeOrNegativeDelta && statuses.size()>6)
			{
				//One chunk
				len+=2;
				//Remove next 7
				for (DWORD i=0;i<7;++i)
					//Remove
					statuses.pop_front();
				//REset
				lastStatus = PacketStatus::Reserved;
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
				// We need to restore the values, as there may be more elements on the buffer
				for (auto it = statuses.begin(); it!=statuses.end(); ++it)
				{
					//Get status
					status = *it;
					//If it is bigger
					if (status>maxStatus)
						//Store it
						maxStatus = status;
					//Check if it is the same
					if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
						//not the same
						allsame = false;
					//Store las status
					lastStatus = status;
				}
			} else if (statuses.size()>13) {
				//One chunk
				len+=2;
				//Remove last 14
				for (DWORD i=0;i<14;++i)
					//Remove
					statuses.pop_front();
				//REset
				lastStatus = PacketStatus::Reserved;
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
			}
		}
	}
	
	//If not finished yet
	if (statuses.size()>0)
		//One chunk more
		len+=2;
	
	//Write now the deltas
	for (std::list<int>::iterator it = deltas.begin(); it!=deltas.end(); ++it)
	{
		//Check size
		if (*it<0 || *it>127)
		{
			//2 bytes
			//Inc
			len += 2;
		} else {
			//1 byte
			//Inc
			len ++;
		}
	}

	//Add zero padding
	if (len%4)
		//DWORD boundary
		len += 4 - (len%4);
	
	//Done
	return len;
}

DWORD RTCPRTPFeedback::TransportWideFeedbackMessageField::Serialize(BYTE* data,DWORD size) const
{
	//If we have no packets
	if (packets.size()==0)
		return 0;
	
	//Calculate temporal info
	WORD baseSeqNumber	= packets.begin()->first;
	bool firstReceived	= false;
	QWORD referenceTime	= 0;
	WORD packetStatusCount	= packets.size();

	/*
		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	       |      base sequence number     |      packet status count      |
	       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	       |                 reference time                | fb pkt. count |
	       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+	
	 */
	//Set data
	set2(data,0,baseSeqNumber);
	set2(data,2,packetStatusCount);
	//Set3 referenceTime when first received
	set1(data,7,feedbackPacketCount);
	
	//Initial time in us
	QWORD time = 0;
	
	//Store delta array
	std::list<int> deltas;
	std::list<PacketStatus> statuses;
	PacketStatus lastStatus = PacketStatus::Reserved;
	PacketStatus maxStatus = PacketStatus::NotReceived;
	bool allsame = true;
	
	//Bitwritter for the rest
	BitWritter writter(data+8,size-8);
	
	//For each packet 
	for (Packets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
	{
		PacketStatus status = PacketStatus::NotReceived;
		
		//If got packet
		if (it->second)
		{
			int delta = 0;
			//If first received
			if (!firstReceived)
			{
				//Got it 
				firstReceived = true;
				//Set it as 3 bytes signed integer
				referenceTime = (it->second/64000) & 0x7FFFFF;
				//Get initial time
				time = referenceTime * 64000;
				//also in bufffer
				set3(data,4,referenceTime);
			}
			
			//Get delta
			if (it->second>time)
				delta = (it->second-time)/250;
			else
				delta = -(int)((time-it->second)/250);
			//If it is negative or to big
			if (delta<0 || delta> 127)
				//Big one
				status = PacketStatus::LargeOrNegativeDelta;
			else
				//Small
				status = PacketStatus::SmallDelta;
			//Store delta
			deltas.push_back(delta);
			//Set next time
			time = time + delta*250;
		}
		
		//Check if all previoues ones were equal and this one the firt different
		if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
		{
			//Anything bigger is worth a run
			if (statuses.size()>7)
			{
				//Write run!
				/*
					0                   1
					0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				       |T| S |       Run Length        |
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					T = 0
				 */
				writter.Put(1,0);
				writter.Put(2,lastStatus);
				writter.Put(13,statuses.size());
				//Remove all statuses
				statuses.clear();
				//Reset status
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
			} else {
				//Not same
				allsame = false;
			}
		}
		
		//Push back statuses, it will be handled later
		statuses.push_back(status);
		
		//If it is bigger
		if (status>maxStatus)
			//Store it
			maxStatus = status;
		//Store las status
		lastStatus = status;

		//If they are different already
		if (!allsame)
		{
			//Check 
			if (maxStatus==PacketStatus::LargeOrNegativeDelta && statuses.size()>6)
			{
				/*
					0                   1
					0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				       |T|S|        Symbols            |
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					T = 1
					S = 1
				 */
				writter.Put(1,1);
				writter.Put(1,1);
				//Set next 7
				for (DWORD i=0;i<7;++i)
				{
					//Write
					writter.Put(2,(BYTE)statuses.front());
					//Remove
					statuses.pop_front();
				}
				//REset
				lastStatus = PacketStatus::Reserved;
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
				// We need to restore the values, as there may be more elements on the buffer
				for (auto it = statuses.begin(); it!=statuses.end(); ++it)
				{
					//Get status
					status = *it;
					//If it is bigger
					if (status>maxStatus)
						//Store it
						maxStatus = status;
					//Check if it is the same
					if (allsame && lastStatus!=PacketStatus::Reserved && status!=lastStatus)
						//not the same
						allsame = false;
					//Store las status
					lastStatus = status;
				}
			} else if (statuses.size()>13) {
				/*
					0                   1
					0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				       |T|S|       symbol list         |
				       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					 T = 1
					 S = 0
				 */
				writter.Put(1,1);
				writter.Put(1,0);
				//Set next 7
				for (DWORD i=0;i<14;++i)
				{
					//Write
					writter.Put(1,(BYTE)statuses.front());
					//Remove
					statuses.pop_front();
				}
				//REset
				lastStatus = PacketStatus::Reserved;
				maxStatus = PacketStatus::NotReceived;
				allsame = true;
			} 
		}
	}
	
	//If not finished yet
	if (statuses.size()>0)
	{
		//How big was the same run
		if (allsame)
		{
			//Write run!
			writter.Put(1,0);
			writter.Put(2,lastStatus);
			writter.Put(13,statuses.size());
		} else if (maxStatus==PacketStatus::LargeOrNegativeDelta) {
			//Write chunk
			writter.Put(1,1);
			writter.Put(1,1);
			//Wirte rest
			for (std::list<PacketStatus>::iterator it = statuses.begin(); it!= statuses.end(); ++it)
				//Write
				writter.Put(2,(BYTE)*it);
			//Write pending
			writter.Put(14-statuses.size()*2,0);
		} else {
			//Write chunck
			writter.Put(1,1);
			writter.Put(1,0);
			//Wirte rest
			for (std::list<PacketStatus>::iterator it = statuses.begin(); it!= statuses.end(); ++it)
				//Write
				writter.Put(1,(BYTE)*it);
			//Write pending
			writter.Put(14-statuses.size(),0);
		}
	}
	
	//Flush wirtter and aling, count also header
	DWORD len = writter.Flush()+8;
	
	//Write now the deltas
	for (std::list<int>::iterator it = deltas.begin(); it!=deltas.end(); ++it)
	{
		//Check size
		if (*it<0 || *it>127)
		{
			//2 bytes
			set2(data,len,(short)*it);
			//Inc
			len += 2;
		} else {
			//1 byte
			set1(data,len,(BYTE)*it);
			//Inc
			len ++;
		}
	}

	//Add zero padding
	while (len%4)
		//Add padding
		data[len++] = 0;
	//Done
	return len;
}

DWORD RTCPRTPFeedback::TransportWideFeedbackMessageField::Parse(const BYTE* data,DWORD size)
{
	//Create the  status count
	std::vector<PacketStatus> statuses;
	
	if (size<8) return 0;
	
	//This are temporal, only packet list count
	//We use DWORDs instead of WORD to allow sequence wrapping without perturving maps
	DWORD baseSeqNumber	= get2(data,0);
	WORD packetStatusCount	= get2(data,2);
	//Get reference time
	referenceTime		= get3(data,4);
	//Store packet count
	feedbackPacketCount	= get1(data,7);

	//Rseserve initial space
	statuses.reserve(packetStatusCount);

	//Where we are 
	DWORD len = 8;
	//Get all
	while (statuses.size()<packetStatusCount)
	{
		//Ensure we have enought
		if (len+2>size)
			return 0;
		
		//Get chunk
		WORD chunk = get2(data,len);
		//Skip it
		len += 2;
		
		//Check packet type
		if (chunk>>15) 
		{
			//Debug("-Chunk %x\n",chunk);
			/*
				0                   1
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			       |T|S|       symbol list         |
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				 T = 1
			*/ 
			//Get status size
			if (chunk>>14 & 1)
			{
				//Debug("-2 bits states\n");
				//S=1 => 7 states, 2 bits per state
				for (DWORD j=0;j<7;++j)
				{
					//Get status
					PacketStatus status = (PacketStatus)((chunk >> 2 * (7 - 1 - j)) & 0x03);
					//Debug("-status %d\n",status);
					//Push it back
					statuses.push_back(status);
					
				}
			} else {
				//Debug("-1 bits states\n");
				//S=> 14 states, 1 bit per state
				for (DWORD j=0;j<14;++j)
				{
					//Get status
					PacketStatus status = (PacketStatus)((chunk >> (14 - 1 - j)) & 0x01);
					//Debug("-status %d\n",status);
					//Push it back
					statuses.push_back(status);
					
				}
			}

		} else {
			
			/*
				0                   1
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			       |T| S |       Run Length        |
			       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				T = 0
			 */
			//Get status
			PacketStatus status = (PacketStatus)(chunk>>13) ;
			//Run lengh
			WORD run = chunk & 0x1FFF;
			//Debug("-run %d status %d\n",run,status);
			//For eachone
			for (WORD j=0;j<run;++j)
				//Append it
				statuses.push_back(status);
		}
	}
	QWORD time = referenceTime * 64000;
	//::Dump4(data+len,size-len);
	//For each packet
	for (DWORD i=0;i<packetStatusCount;++i)
	{
		//Depending on the status
		switch (statuses[i])
		{
			case PacketStatus::NotReceived:
				//Append not received
				packets[baseSeqNumber+i] = 0;
				break;
			case PacketStatus::SmallDelta:
			{
				//Check size
				if (len+1>size)
					return 0;
				//Read 1 length delta
				int delta = get1(data,len)* 250 ;
				//Add it to time
				time += delta;
				//Increase delta
				len += 1;
				//Append it
				packets[baseSeqNumber+i] = time;
				break;
			}
			case PacketStatus::LargeOrNegativeDelta:
			{
				//Check size
				if (len+2>size)
					return 0;
				//Get short delta
				short aux = get2(data,len);
				//Read 2 length delta as signed short
				int delta = aux * 250 ;
				len += 2;
				//Increase delta
				time += delta;
				//Append it
				packets[baseSeqNumber+i] = time;
				break;	
			}
			case PacketStatus::Reserved:
				//Ignore
				break;
		}
	}
	//Skip zero padding
	if (len%4)
		//DWORD boundary
		len += 4 - (len%4);
	//Parsed
	return len;
}

void RTCPRTPFeedback::TransportWideFeedbackMessageField::Dump() const
{
	QWORD prev = 0;
	//Debug
	Debug("\t\t[TransportWideFeedbackMessage seq:%d num:%llu]\n",feedbackPacketCount,packets.size());
	//For each packet
	for (Packets::const_iterator it = packets.begin(); it!=packets.end(); ++it)
	{
		//DEbug
		Debug("\t\t\t[Packet seq:%u time=%llu diff=%.6lld/]\n",it->first,it->second, prev && it->second ? it->second-prev : 0);
		if (it->second>prev) prev = it->second;
	}
	//Debug
	Debug("\t\t[TransportWideFeedbackMessage/]\n");
}
