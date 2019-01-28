#include "rtp/RTPIncomingSourceGroup.h"

#include <math.h>

#include "VideoLayerSelector.h"
#include "remoterateestimator.h"

RTPIncomingSourceGroup::RTPIncomingSourceGroup(MediaFrame::Type type,TimeService& timeService) :
	timeService(timeService),
	losts(128)
{
	//Small initial bufer of 100ms
	packets.SetMaxWaitTime(100);
	//LIsten remote rate events
	remoteRateEstimator.SetListener(this);
	//Create dispatch timer
	dispatchTimer = timeService.CreateTimer([this](auto now){ DispatchPackets(now.count()); });
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
		
	ScopedLock scoped(listenerMutex);
	listeners.insert(listener);
}

void RTPIncomingSourceGroup::RemoveListener(Listener* listener) 
{
	Debug("-RTPIncomingSourceGroup::RemoveListener() [listener:%p]\n",listener);
		
	ScopedLock scoped(listenerMutex);
	listeners.erase(listener);
}

int RTPIncomingSourceGroup::AddPacket(const RTPPacket::shared &packet, DWORD size)
{
	
	//UltraDebug(">RTPIncomingSourceGroup::AddPacket()\n");
	
	//Check if it is the rtx packet used to calculate rtt
	if (rttrtxTime && packet->GetSeqNum()==rttrtxSeq)
	{
		//Get packet time
		const auto time = packet->GetTime();
		//Calculate rtt
		const auto rtt = time - rttrtxTime;
		//Set RTT
		SetRTT(rtt);
		//Done
		return 0;
	}
	
	//If it is not from the media ssrc
	if (packet->GetSSRC()!=media.ssrc)
		//Do nothing else
		return 0;
	
	//Add to lost packets and get count regardeless if this was discarded or not
	auto lost = losts.AddPacket(packet);
	
	//If doing remb
	if (remb)
	{
		//Add estimation
		remoteRateEstimator.Update(media.ssrc,packet,size);
		//Update lost
		if (lost) remoteRateEstimator.UpdateLost(media.ssrc,lost,getTimeMS());
	}
	
	//Add to packet queue
	if (!packets.Add(packet))
		//Rejected packet
		return -1;
	
	//Get next time for dispatcht
	uint64_t next = packets.GetWaitTime(getTimeMS());
	
	//UltraDebug("-RTPIncomingSourceGroup::AddPacket() | [lost:%d,next:%llu]\n",lost,next);
	
	//If we have anything on the queue
	if (next!=(QWORD)-1)
		//Reschedule timer
		dispatchTimer->Again(std::chrono::milliseconds(next));
	
	//Return lost packets
	return lost;
}

void RTPIncomingSourceGroup::ResetPackets()
{
	//Lock sources accumulators
	ScopedLock scoped(mutex);
	
	//Reset packet queue and lost count
	packets.Reset();
	losts.Reset();
	//Reset stats
	lost = 0;
	minWaitedTime = 0;
	maxWaitedTime = 0;
	avgWaitedTime = 0;
}

void RTPIncomingSourceGroup::Update()
{
	Update(getTimeMS());
}

void RTPIncomingSourceGroup::Update(QWORD now)
{
	//Lock sources accumulators
	ScopedLock scoped(mutex);
	
	//Refresh instant bitrates
	media.acumulator.Update(now);
	rtx.acumulator.Update(now);
	fec.acumulator.Update(now);
	//Update also all media layers
	for (auto& entry : media.layers)
		//Update bitrate also
		entry.second.acumulator.Update(now);
	//Update stats
	lost = GetCurrentLost();
	minWaitedTime = GetMinWaitedTime();
	maxWaitedTime = GetMaxWaitedTime();
	avgWaitedTime = GetAvgWaitedTime();
}

void RTPIncomingSourceGroup::SetRTT(DWORD rtt)
{
	//Store rtt
	this->rtt = rtt;
	//Set max packet wait time
	packets.SetMaxWaitTime(fmin(500,fmax(120,rtt*4)+40));
	//If using remote rate estimator
	if (remb)
		//Add estimation
		remoteRateEstimator.UpdateRTT(media.ssrc,rtt,getTimeMS());
}

WORD RTPIncomingSourceGroup::SetRTTRTX(uint64_t time)
{
	//Get max received packet, the ensure it has not been nacked
	WORD last = media.extSeqNum & 0xffff;
	//Update sent time
	rttrtxSeq = last;
	rttrtxTime = time/1000;
	//Return last
	return last;
}

void RTPIncomingSourceGroup::Start(bool remb)
{
	Debug("-RTPIncomingSourceGroup::Start() | [remb:%d]\n",remb);
	
	//are we using remb?
	this->remb = remb;
	
	//Add media ssrc
	remoteRateEstimator.AddStream(media.ssrc);
}

void RTPIncomingSourceGroup::DispatchPackets(uint64_t time)
{
	//UltraDebug("-RTPIncomingSourceGroup::DispatchPackets() | [time:%llu]\n",time);
	
	//Block listeners
	ScopedLock scoped(listenerMutex);
	
	//Deliver all pending packets at once
	for (auto packet = packets.GetOrdered(getTimeMS()); packet; packet = packets.GetOrdered(getTimeMS()))
	{
		//We need to adjust the seq num due the in band probing packets
		packet->SetExtSeqNum(packet->GetExtSeqNum() - packets.GetNumDiscardedPackets());
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onRTP(this,packet);
	}
}

void RTPIncomingSourceGroup::Stop()
{
	ScopedLock scoped(listenerMutex);
	
	//Deliver to all listeners
	for (auto listener : listeners)
		//Dispatch rtp packet
		listener->onEnded(this);
	//Clear listeners
	listeners.clear();
}

RTPIncomingSource* RTPIncomingSourceGroup::Process(RTPPacket::shared &packet)
{
	//Get packet time
	uint64_t time = packet->GetTime();
	
	//Update instant bitrates
	Update(time);
	
	//Get ssrc
	uint32_t ssrc = packet->GetSSRC();
	
	//Get source for ssrc
	RTPIncomingSource* source = GetSource(ssrc);
	
	//Ensure it has a source
	if (!source)
		//error
		return nullptr;

	//if it is video
	if (type == MediaFrame::Video)
	{
		//Check if we can ge the layer info
		auto info = VideoLayerSelector::GetLayerIds(packet);
		//UltraDebug("-VideoLayerSelector::GetLayerIds() | [id:%x,tid:%u,sid:%u]\n",info.GetId(),info.temporalLayerId,info.spatialLayerId);
		//Lock sources accumulators
		ScopedLock scoped(mutex);
		//Update source and layer info
		source->Update(time, packet->GetSeqNum(), packet->GetRTPHeader().GetSize() + packet->GetMediaLength(), info);
	} else {
		//Lock sources accumulators
		ScopedLock scoped(mutex);
		//Update source and layer info
		source->Update(time, packet->GetSeqNum(), packet->GetRTPHeader().GetSize() + packet->GetMediaLength());
	}
	
	//Update source sequence number and get cycles
	WORD cycles = source->SetSeqNum(packet->GetSeqNum());

	//Set cycles back
	packet->SetSeqCycles(cycles);
	
	//Done
	return source;
}


void RTPIncomingSourceGroup::onTargetBitrateRequested(DWORD bitrate)
{
	UltraDebug("-RTPIncomingSourceGroup::onTargetBitrateRequested() | [bitrate:%d]\n",bitrate);
	//store estimation
	remoteBitrateEstimation = bitrate;
}
