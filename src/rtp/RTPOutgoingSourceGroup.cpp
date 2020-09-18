#include "rtp/RTPOutgoingSourceGroup.h"


RTPOutgoingSourceGroup::RTPOutgoingSourceGroup(MediaFrame::Type type)
{
	this->type = type;
}

RTPOutgoingSourceGroup::RTPOutgoingSourceGroup(std::string &mid,MediaFrame::Type type)
{
	this->mid = mid;
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
	
	ScopedLock scoped(listenersMutex);
	listeners.insert(listener);
	
}

void RTPOutgoingSourceGroup::RemoveListener(Listener* listener) 
{
	Debug("-RTPOutgoingSourceGroup::RemoveListener() [listener:%p]\n",listener);
	
	ScopedLock scoped(listenersMutex);
	listeners.erase(listener);
}

void RTPOutgoingSourceGroup::ReleasePackets(QWORD until)
{
	//Delete old packets
	auto it = packets.begin();
	//Until the end
	while(it!=packets.end())
	{
		//Check packet time
		if (it->second->GetTime()>=until)
			//Keep the rest
			break;
		//Delete from queue and move next
		packets.erase(it++);
	}
}

void RTPOutgoingSourceGroup::ReleasePacketsByTimestamp(QWORD until)
{
	//Delete old packets
	auto it = packets.begin();
	//Until the end
	while(it!=packets.end())
	{
		//Check packet timestamp
		if (it->second->GetExtTimestamp()>=until)
			//Keep the rest
			break;
		//Delete from queue and move next
		packets.erase(it++);
	}
}

void RTPOutgoingSourceGroup::ReleaseAllPackets()
{
	//Clear
	packets.clear();
}
	
void RTPOutgoingSourceGroup::AddPacket(const RTPPacket::shared& packet)
{
	//Add a clone to the rtx queue
	packets[packet->GetExtSeqNum()] = packet;
}

RTPPacket::shared RTPOutgoingSourceGroup::GetPacket(WORD seq) const
{
	//If there are no packets
	if (packets.empty())
	{
		//Debug
		UltraDebug("-RTPOutgoingSourceGroup::GetPacket() | no packets available\n");
		//Not found
		return nullptr;
	}
	
	//Check sequence wrap
	WORD cycles = media.cycles;
	
	//IF there is too much difference between first in queue and requested sequence
	if ((packets.begin()->first & 0xFFFF)<0x0FFF && seq>0xF000)
		//It was from the past cycle
		cycles--;
	
	//Get normal sequence
	DWORD ext = ((DWORD)(cycles)<<16 | seq);
	
	//Find packet to retransmit
	auto it = packets.find(ext);

	//If we don't have it
	if (it==packets.end())
	{
		//Debug
		UltraDebug("-RTPOutgoingSourceGroup::GetPacket() | packet not found [seqNum:%u,extSeqNum:%u,cycles:%u,media:%u,first:%u,num:%u]\n",seq,ext,cycles,media.cycles,packets.begin()->first,packets.size());
		//Not found
		return nullptr;
	}
	
	//Get packet
	return  it->second;
}

void RTPOutgoingSourceGroup::onPLIRequest(DWORD ssrc)
{
	ScopedLock scoped(listenersMutex);
	for (auto listener : listeners)
		listener->onPLIRequest(this,ssrc);
}

void RTPOutgoingSourceGroup::onREMB(DWORD ssrc, DWORD bitrate)
{
	//Update remb on media
	media.remb = bitrate;
	
	ScopedLock scoped(listenersMutex);
	for (auto listener : listeners)
		listener->onREMB(this,ssrc,bitrate);
}

void RTPOutgoingSourceGroup::Update()
{
	Update(getTimeMS());
}

void RTPOutgoingSourceGroup::Update(QWORD now)
{
	//Update media
	{
		//Lock source
		ScopedLock scoped(media);
		//Update
		media.Update(now);
	}
	
	//Update RTX
	{
		//Lock source
		ScopedLock scoped(rtx);
		//Update
		rtx.Update(now);
	}
	
	//Update FEC
	{
		//Lock source
		ScopedLock scoped(fec);
		//Update
		fec.Update(now);
	}
}
