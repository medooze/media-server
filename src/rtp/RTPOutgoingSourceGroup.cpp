#include "rtp/RTPOutgoingSourceGroup.h"


RTPOutgoingSourceGroup::RTPOutgoingSourceGroup(MediaFrame::Type type, TimeService& timeService) :
	timeService(timeService)
{
	this->type = type;
}

RTPOutgoingSourceGroup::RTPOutgoingSourceGroup(const std::string &mid, MediaFrame::Type type, TimeService& timeService) :
	timeService(timeService)
{
	this->mid = mid;
	this->type = type;
}

RTPOutgoingSourceGroup::~RTPOutgoingSourceGroup()
{
	//If there are any listener
	if (listeners.size())
		//Stop again
		Stop();
}

RTPOutgoingSource* RTPOutgoingSourceGroup::GetSource(DWORD ssrc)
{
	if (ssrc == media.ssrc)
		return &media;
	else if (ssrc == rtx.ssrc)
		return &rtx;
	return NULL;
}

void RTPOutgoingSourceGroup::AddListener(Listener* listener) 
{
	Debug("-RTPOutgoingSourceGroup::AddListener() [listener:%p]\n",listener);
	
	//Add it sync
	timeService.Sync([=](auto) {
		listeners.insert(listener);
	});
	
}

void RTPOutgoingSourceGroup::RemoveListener(Listener* listener) 
{
	Debug("-RTPOutgoingSourceGroup::RemoveListener() [listener:%p]\n",listener);
	
	//Remove it sync
	timeService.Sync([=](auto) {
		listeners.erase(listener);
	});
}

void RTPOutgoingSourceGroup::AddPacket(const RTPPacket::shared& packet)
{
	//Add to the rtx queue
	packets.Set(packet->GetSeqNum(), packet);
}

RTPPacket::shared RTPOutgoingSourceGroup::GetPacket(WORD seq) const
{
	//If there are no packets
	if (!packets.GetLength())
	{
		//Debug
		UltraDebug("-RTPOutgoingSourceGroup::GetPacket() | no packets available\n");
		//Not found
		return nullptr;
	}
	
	//Find packet to retransmit
	auto packet = packets.Get(seq);

	//If we don't have it
	if (!packet)
	{
		//Debug
		UltraDebug("-RTPOutgoingSourceGroup::GetPacket() | packet not found [seqNum:%u,media:%u,first:%u,last:%u]\n",seq,media.cycles,packets.GetFirstSeq(), packets.GetLastSeq());
		//Not found
		return nullptr;
	}
	
	//Get packet
	return packet.value();
}

void RTPOutgoingSourceGroup::onPLIRequest(DWORD ssrc)
{
	//Send asycn
	timeService.Async([=](auto) {
		//Deliver to all listeners
		for (auto listener : listeners)
			listener->onPLIRequest(this,ssrc);
	});
}

void RTPOutgoingSourceGroup::onREMB(DWORD ssrc, DWORD bitrate)
{
	//Update remb on media
	media.remb = bitrate;
	
	//Send asycn
	timeService.Async([=](auto) {
		//Deliver to all listeners
		for (auto listener : listeners)
			listener->onREMB(this,ssrc,bitrate);
	});
}

void RTPOutgoingSourceGroup::Update()
{
	//Update it sync
	timeService.Sync([=](auto now) {
		//Set last updated time
		lastUpdated = now.count();
		//Update
		media.Update(now.count());
		//Update
		rtx.Update(now.count());
	});
}

void RTPOutgoingSourceGroup::Update(QWORD now)
{
	//Update it sync
	timeService.Sync([=](auto) {
		//Set last updated time
		lastUpdated = now;
		//Update
		media.Update(now);
		//Update
		rtx.Update(now);
	});
}


void RTPOutgoingSourceGroup::Stop()
{
	Debug("-RTPOutgoingSourceGroup::Stop()\r\n");

	//Add it sync
	timeService.Sync([=](auto) {
		//Signal them we have been ended
		for (auto listener : listeners)
			listener->onEnded(this);
		//No mor listeners
		listeners.clear();
	});

}