#include <arpa/inet.h>
#include <stdlib.h>
#include "log.h"
#include "rtp.h"
#include "bitstream.h"

	
RTCPSenderReport* RTPOutgoingSource::CreateSenderReport(QWORD now)
{
	//Create Sender report
	RTCPSenderReport *sr = new RTCPSenderReport();

	//Append data
	sr->SetSSRC(ssrc);
	sr->SetTimestamp(now);
	sr->SetRtpTimestamp(lastTime);
	sr->SetOctectsSent(totalBytes);
	sr->SetPacketsSent(numPackets);
	
	//Store last sending time
	lastSenderReport = now;
	//Store last send SR 32 middle bits
	lastSenderReportNTP = sr->GetNTPTimestamp();
	
	//Return it
	return sr;
}

 
RTCPReport *RTPIncomingSource::CreateReport(QWORD now)
{
	//If we have received somthing
	if (!totalPacketsSinceLastSR || !(extSeq>=minExtSeqNumSinceLastSR))
		//Nothing to report
		return NULL;
	

	//Get number of total packtes
	DWORD total = extSeq - minExtSeqNumSinceLastSR + 1;
	//Calculate lost
	DWORD lostPacketsSinceLastSR = total - totalPacketsSinceLastSR;
	//Add to total lost count
	lostPackets += lostPacketsSinceLastSR;
	//Calculate fraction lost
	DWORD frac = (lostPacketsSinceLastSR*256)/total;

	//Calculate time since last report
	DWORD delaySinceLastSenderReport = 0;
	//Double check it is correct 
	if (lastReceivedSenderReport && now > lastReceivedSenderReport)
		//Get diff in ms
		delaySinceLastSenderReport = (now - lastReceivedSenderReport)/1000;
	//Create report
	RTCPReport *report = new RTCPReport();

	//Set SSRC of incoming rtp stream
	report->SetSSRC(ssrc);

	//Get time and update it
	report->SetDelaySinceLastSRMilis(delaySinceLastSenderReport);
	// The middle 32 bits out of 64 in the NTP timestamp (as explained in Section 4) 
	// received as part of the most recent RTCP sender report (SR) packet from source SSRC_n.
	// If no SR has been received yet, the field is set to zero.
	//Other data
	report->SetLastSR(lastReceivedSenderNTPTimestamp >> 16);
	report->SetFractionLost(frac);
	report->SetLastJitter(jitter);
	report->SetLostCount(lostPackets);
	report->SetLastSeqNum(extSeq);

	//Reset data
	lastReport = now;
	totalPacketsSinceLastSR = 0;
	totalBytesSinceLastSR = 0;
	minExtSeqNumSinceLastSR = RTPPacket::MaxExtSeqNum;

	//Return it
	return report;
}
	
RTPLostPackets::RTPLostPackets(WORD num)
{
	//Store number of packets
	size = num;
	//Create buffer
	packets = (QWORD*) malloc(num*sizeof(QWORD));
	//Set to 0
	memset(packets,0,size*sizeof(QWORD));
	//No first packet
	first = 0;
	//None yet
	len = 0;
	total = 0;
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

WORD RTPLostPackets::AddPacket(const RTPPacket *packet)
{
	int lost = 0;
	
	//Get the packet number
	DWORD extSeq = packet->GetExtSeqNum();
	
	//Check if is befor first
	if (extSeq<first)
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
		len = size;
		//We are last
		pos = size-1;
	} 
	
	//Check if it is last
	if (len-1<=pos)
	{
		//lock until we find a non lost and increase counter in the meanwhile
		for (int i=pos-1;i>=0 && !packets[i];--i)
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


std::list<RTCPRTPFeedback::NACKField*> RTPLostPackets::GetNacks()
{
	std::list<RTCPRTPFeedback::NACKField*> nacks;
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
				nacks.push_back(new RTCPRTPFeedback::NACKField(lost,mask));
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
		nacks.push_back(new RTCPRTPFeedback::NACKField(lost,mask));
	
	return nacks;
}

void  RTPLostPackets::Dump()
{
	Debug("[RTPLostPackets size=%d first=%d len=%d]\n",size,first,len);
	for(int i=0;i<len;i++)
		Debug("[%.3d,%.8d]\n",i,packets[i]);
	Debug("[/RTPLostPackets]\n");
}


RTPOutgoingSourceGroup::RTPOutgoingSourceGroup(MediaFrame::Type type)
{
	this->type = type;
}

RTPOutgoingSourceGroup::RTPOutgoingSourceGroup(std::string &streamId,MediaFrame::Type type)
{
	this->streamId = streamId;
	this->type = type;
}

void RTPOutgoingSourceGroup::AddListener(Listener* listener) 
{
	Debug("-RTPOutgoingSourceGroup::AddListener() [listener:%p]\n",listener);
	
	ScopedLock scoped(mutex);
	listeners.insert(listener);
	
}

void RTPOutgoingSourceGroup::RemoveListener(Listener* listener) 
{
	Debug("-RTPOutgoingSourceGroup::RemoveListener() [listener:%p]\n",listener);
	
	ScopedLock scoped(mutex);
	listeners.insert(listener);
}

void RTPOutgoingSourceGroup::onPLIRequest(DWORD ssrc)
{
	ScopedLock scoped(mutex);
	for (Listeners::const_iterator it=listeners.begin();it!=listeners.end();++it)
		(*it)->onPLIRequest(this,ssrc);
}

RTPIncomingSourceGroup::RTPIncomingSourceGroup(MediaFrame::Type type) 
	: losts(64)
{
	this->type = type;
	//Small bufer of 20ms
	packets.SetMaxWaitTime(20);
}

RTPIncomingSource* RTPIncomingSourceGroup::GetSource(DWORD ssrc)
{
	if (ssrc == media.ssrc)
		return &media;
	else if (ssrc == rtx.ssrc)
		return &rtx;
	else if (ssrc == fec.ssrc)
		return &fec;
	return NULL;
}

void RTPIncomingSourceGroup::AddListener(Listener* listener) 
{
	Debug("-RTPIncomingSourceGroup::AddListener() [listener:%p]\n",listener);
		
	ScopedLock scoped(mutex);
	listeners.insert(listener);
}

void RTPIncomingSourceGroup::RemoveListener(Listener* listener) 
{
	Debug("-RTPIncomingSourceGroup::RemoveListener() [listener:%p]\n",listener);
		
	ScopedLock scoped(mutex);
	listeners.insert(listener);
}

void RTPIncomingSourceGroup::onRTP(RTPPacket* packet)
{
	ScopedLock scoped(mutex);
	for (Listeners::const_iterator it=listeners.begin();it!=listeners.end();++it)
		(*it)->onRTP(this,packet);
	delete(packet);
}
