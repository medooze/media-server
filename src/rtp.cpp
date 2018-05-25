#include <arpa/inet.h>
#include <stdlib.h>
#include "log.h"
#include "rtp.h"
#include "bitstream.h"

BYTE LayerInfo::MaxLayerId = 0xFF;

RTCPSenderReport::shared RTPOutgoingSource::CreateSenderReport(QWORD now)
{
	//Create Sender report
	auto sr = std::make_shared<RTCPSenderReport>();

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

 
RTCPReport::shared RTPIncomingSource::CreateReport(QWORD now)
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
	RTCPReport::shared report = std::make_shared<RTCPReport>();

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

WORD RTPLostPackets::AddPacket(const RTPPacket::shared &packet)
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

RTPOutgoingSource* RTPOutgoingSourceGroup::GetSource(DWORD ssrc)
{
	if (ssrc == media.ssrc)
		return &media;
	else if (ssrc == rtx.ssrc)
		return &rtx;
	else if (ssrc == fec.ssrc)
		return &fec;
	return NULL;
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
	listeners.erase(listener);
}

void RTPOutgoingSourceGroup::ReleasePackets(QWORD until)
{
	//Lock packets
	ScopedLock scoped(mutex);
	
	//Delete old packets
	auto it = packets.begin();
	//Until the end
	while(it!=packets.end())
	{
		//Check packet time
		if (it->second->GetTime()>until)
			//Keep the rest
			break;
		//Delete from queue and move next
		packets.erase(it++);
	}
}

void RTPOutgoingSourceGroup::AddPacket(const RTPPacket::shared& packet)
{
	//Lock packets
	ScopedLock scoped(mutex);
	
	//Add a clone to the rtx queue
	packets[packet->GetExtSeqNum()] = packet;
}

RTPPacket::shared RTPOutgoingSourceGroup::GetPacket(WORD seq) const
{
	//Lock packets
	ScopedLock scoped(mutex);
	
	//Consider seq wrap
	DWORD ext = ((DWORD)(media.cycles)<<16 | seq);
	
	//Find packet to retransmit
	auto it = packets.find(ext);

	//If we don't have it
	if (it==packets.end())
		//None
		return nullptr;
	
	//Get packet
	return  it->second;
}

void RTPOutgoingSourceGroup::onPLIRequest(DWORD ssrc)
{
	ScopedLock scoped(mutex);
	for (auto listener : listeners)
		listener->onPLIRequest(this,ssrc);
}

RTPIncomingSourceGroup::RTPIncomingSourceGroup(MediaFrame::Type type) 
	: losts(128)
{
	this->rtt = 0;
	this->rttrtxSeq = 0;
	this->rttrtxTime = 0;
	this->type = type;
	//Small initial bufer of 60ms
	packets.SetMaxWaitTime(60);
}

RTPIncomingSourceGroup::~RTPIncomingSourceGroup() 
{
	Stop();
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
	listeners.erase(listener);
}

int RTPIncomingSourceGroup::AddPacket(const RTPPacket::shared &packet)
{
	//Add to lost packets and get count regardeless if this was discarded or not
	auto lost = losts.AddPacket(packet);
	//Add to packet queue
	if (!packets.Add(packet))
		//Rejected packet
		return -1;
	//Return lost packets
	return lost;
}

void RTPIncomingSourceGroup::ResetPackets()
{
	//Reset packet queue and lost count
	packets.Reset();
	losts.Reset();
}

void RTPIncomingSourceGroup::Update(QWORD now)
{
	//Refresh instant bitrates
	media.bitrate.Update(now);
	rtx.bitrate.Update(now);
	fec.bitrate.Update(now);
	//Update also all media layers
	for (auto& entry : media.layers)
		//Update bitrate also
		entry.second.bitrate.Update(now);
}

void RTPIncomingSourceGroup::SetRTT(DWORD rtt)
{
	//Store rtt
	this->rtt = rtt;
	//If we have a rtx stream
	if (rtx.ssrc)
		//Set max packet wait time
		packets.SetMaxWaitTime(fmin(500,fmax(60,rtt*4)+40));
}

void RTPIncomingSourceGroup::Start()
{
	if (!isZeroThread(dispatchThread))
		return;
	
	//Create dispatch trhead
	createPriorityThread(&dispatchThread,[](void* arg) -> void* {
			Log(">RTPIncomingSourceGroup dispatch %p\n",arg);
			//Get group
			RTPIncomingSourceGroup* group = (RTPIncomingSourceGroup*) arg;
			//Loop until canceled
			RTPPacket::shared packet;
			while ((packet=group->packets.Wait()))
			{
				ScopedLock scoped(group->mutex);
				//Deliver to all listeners
				for (auto listener : group->listeners)
					//Dispatch rtp packet
					listener->onRTP(group,packet);
			}
			Log("<RTPIncomingSourceGroup dispatch\n");
			return nullptr;
		},
		(void*)this,
		0
	);
}

void RTPIncomingSourceGroup::Stop()
{
	if (isZeroThread(dispatchThread))
		return;
	//Cacnel packet wait
	packets.Cancel();
	
	//Wait for thread
	pthread_join(dispatchThread,NULL);
	
	//Clean it
	setZeroThread(&dispatchThread);
	
	ScopedLock scoped(mutex);
	//Deliver to all listeners
	for (auto listener : listeners)
		//Dispatch rtp packet
		listener->onEnded(this);
	//Clear listeners
	listeners.clear();
}