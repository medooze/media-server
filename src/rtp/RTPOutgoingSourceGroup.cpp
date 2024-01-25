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
	else if (ssrc == fec.ssrc)
		return &fec;
	else if (ssrc == rtx.ssrc)
		return &rtx;
	return NULL;
}

void RTPOutgoingSourceGroup::AddListener(Listener* listener) 
{
	Debug("-RTPOutgoingSourceGroup::AddListener() [listener:%p]\n",listener);
	
	//Add it sync
	timeService.Async([=](auto) {
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

void RTPOutgoingSourceGroup::UpdateAsync(std::function<void(std::chrono::milliseconds)> callback)
{
	//Update it sync
	timeService.Async([=](auto now) {
		//Set last updated time
		lastUpdated = now.count();
		//Update
		media.Update(now.count());
		//Update
		fec.Update(now.count());
		//Update
		rtx.Update(now.count());
	}, callback);
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
		fec.Update(now.count());
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
		fec.Update(now);
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

bool RTPOutgoingSourceGroup::isRTXAllowed(WORD seq, QWORD now) const
{
	//If there are no rtx times
	if (!rtxTimes.GetLength())
		//It is the first rtx packet
		return true;

	//Find last rtx time
	auto time = rtxTimes.Get(seq);

	//If we don't have it
	if (!time)
		//First time rtx this packet
		return true;

	//Don't allow to rtx more than twice per rtt
	return time.value() + media.rtt/2 < now;
}

void RTPOutgoingSourceGroup::SetRTXTime(WORD seq, QWORD time)
{
	//Update rtx time for seq
	rtxTimes.Set(seq, time);
}