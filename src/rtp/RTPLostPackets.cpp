#include "rtp/RTPLostPackets.h"

RTPLostPackets::RTPLostPackets(WORD num)
{
	//Store number of packets
	size = num;
	//Create buffer
	packets = (QWORD*) malloc(num*sizeof(QWORD));
	//Set to 0
	memset(packets,0,size*sizeof(QWORD));
}

void RTPLostPackets::Reset()
{
	//Set to 0
	memset(packets,0,size*sizeof(QWORD));
	//No first packet
	first = 0;
	//None yet
	len = 0;
	total = 0;
}

RTPLostPackets::~RTPLostPackets()
{
	free(packets);
}

WORD RTPLostPackets::AddPacket(const RTPPacket::shared &packet)
{
	int lost = 0;
	
	//Get the packet number
	DWORD extSeq = packet->GetExtSeqNum();
	
	//Check if is before first
	if (first && extSeq<first)
		//Exit, very old packet
		return 0;

	//If we are first
	if (!first)
		//Set to us
		first = extSeq;
	       
	//Get our position
	WORD pos = extSeq-first;
	
	//Check if we are still in window
	if (pos+1>size) 
	{
		//How much do we need to remove?
		int n = pos+1-size;
		//Check if we have to much to remove
		if (n>size)
			//cap it
			n = size;
		//Caculate total count
		for (int i=0;i<n;++i)
			//If it was lost
			if (!packets[i])
				//Decrease total
				total--;
		//Move the rest
		memmove(packets,packets+n,(size-n)*sizeof(QWORD));
		//Fill with 0 the new ones
		memset(packets+(size-n),0,n*sizeof(QWORD));
		//Set first
		first = extSeq-size+1;
		//Full
		len = size-1;
		//We are last
		pos = size-1;
	} 
	
	//Check if it is last
	if (len<pos+1)
	{
		//look until we find a non lost and increase counter in the meanwhile
		for (int i=pos; i>0 && !packets[i-1];--i)
			//Lost
			lost++;
		//Increase lost
		total += lost;
		//Update last
		len = pos+1;
	} else {
		//If it was lost
		if (!packets[pos])
			//One lost total less
			total--;
	}
	
	//Set
	packets[pos] = packet->GetTime();
	
	//Return lost ones
	return lost;
}


std::list<RTCPRTPFeedback::NACKField::shared> RTPLostPackets::GetNacks() const
{
	std::list<RTCPRTPFeedback::NACKField::shared> nacks;
	WORD lost = 0;
	WORD mask = 0;
	int n = 0;
	
	//Iterate packets
	for(WORD i=0;i<len;i++)
	{
		//Are we in a lost count?
		if (lost)
		{
			//It was lost?
			if (packets[i]==0)
				//Update mask
				mask |= 1 << n;
			//Increase mask len
			n++;
			//If we are enought
			if (n==16)
			{
				//Add new NACK field to list
				nacks.push_back(std::make_shared<RTCPRTPFeedback::NACKField>(lost,mask));
				//Reset counters
				n = 0;
				lost = 0;
				mask = 0;
			}
		}
		//Is this the first one lost
		else if (!packets[i]) {
			//This is the first one
			lost = first+i;
		}
		
	}
	
	//Are we in a lost count?
	if (lost)
		//Add new NACK field to list
		nacks.push_back(std::make_shared<RTCPRTPFeedback::NACKField>(lost,mask));
	
	return nacks;
}

void  RTPLostPackets::Dump() const
{
	Debug("[RTPLostPackets size=%d first=%d len=%d]\n",size,first,len);
	for(int i=0;i<len;i++)
		Debug("[%.3d,%.8llu]\n",i,packets[i]);
	Debug("[/RTPLostPackets]\n");
}
