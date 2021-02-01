#include "rtp/RTPIncomingSourceGroup.h"

#include <math.h>

#include "VideoLayerSelector.h"
#include "remoterateestimator.h"

using namespace std::chrono_literals;

RTPIncomingSourceGroup::RTPIncomingSourceGroup(MediaFrame::Type type,TimeService& timeService) :
	timeService(timeService),
	losts(256)
{
	//Store type
	this->type = type;
	//Small initial bufer of 100ms
	packets.SetMaxWaitTime(100);
	//LIsten remote rate events
	remoteRateEstimator.SetListener(this);
	//Create dispatch timer
	dispatchTimer = timeService.CreateTimer([this](auto now){ DispatchPackets(now.count()); });
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

void RTPIncomingSourceGroup::AddListener(RTPIncomingMediaStream::Listener* listener) 
{
	Debug("-RTPIncomingSourceGroup::AddListener() [listener:%p]\n",listener);
		
	ScopedLock scoped(listenerMutex);
	listeners.insert(listener);
}

void RTPIncomingSourceGroup::RemoveListener(RTPIncomingMediaStream::Listener* listener) 
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
	
	//Add to lost packets
	auto lost = losts.AddPacket(packet);
	
	//Get now
	auto now = getTimeMS();
	
	//Update instant lost accumulator
	if (lost) media.AddLostPackets(now, lost);
	
	//If doing remb
	if (remb)
	{
		//Add estimation
		remoteRateEstimator.Update(media.ssrc,packet,size);
		//Update lost
		if (lost) remoteRateEstimator.UpdateLost(media.ssrc,lost,now);
	}
	
	//Add to packet queue
	if (!packets.Add(packet))
		//Rejected packet
		return -1;
	
	//Get next time for dispatcht
	uint64_t next = packets.GetWaitTime(now);
	
	//UltraDebug("-RTPIncomingSourceGroup::AddPacket() | [lost:%d,next:%llu]\n",lost,next);
	
	//If we have anything on the queue
	if (next!=(QWORD)-1)
		//Reschedule timer
		dispatchTimer->Again(std::chrono::milliseconds(next));
	
	//Return lost packets
	return lost;
}

void RTPIncomingSourceGroup::Bye(DWORD ssrc)
{
	if (ssrc == media.ssrc)
	{
		//REset packets
		ResetPackets();
		//Reset 
		{
			//Block listeners
			ScopedLock scoped(listenerMutex);
			//Deliver to all listeners
			for (auto listener : listeners)
				//Dispatch rtp packet
				listener->onBye(this);
		}
	} else if (ssrc == rtx.ssrc) {
	} else if (ssrc == fec.ssrc) {
	}
}

void RTPIncomingSourceGroup::ResetPackets()
{
	//Reset packet queue and lost count
	packets.Reset();
	losts.Reset();
}

void RTPIncomingSourceGroup::Update()
{
	Update(getTimeMS());
}

void RTPIncomingSourceGroup::Update(QWORD now)
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

void RTPIncomingSourceGroup::SetRTT(DWORD rtt)
{
	//Store rtt
	this->rtt = rtt;
	//Set max packet wait time
	packets.SetMaxWaitTime(fmin(500,fmax(120,rtt)*2));
	//Dispatch packets with new timer now
	dispatchTimer->Again(0ms);
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
	if (media.ssrc) remoteRateEstimator.AddStream(media.ssrc);
}

void RTPIncomingSourceGroup::DispatchPackets(uint64_t time)
{
	//UltraDebug("-RTPIncomingSourceGroup::DispatchPackets() | [time:%llu]\n",time);
	
	//Deliver all pending packets at once
	std::vector<RTPPacket::shared> ordered;
	for (auto packet = packets.GetOrdered(getTimeMS()); packet; packet = packets.GetOrdered(getTimeMS()))
	{
		//We need to adjust the seq num due the in band probing packets
		packet->SetExtSeqNum(packet->GetExtSeqNum() - packets.GetNumDiscardedPackets());
		//Add to packets
		ordered.push_back(packet);
	}
	
	{
		//Block listeners
		ScopedLock scoped(listenerMutex);
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onRTP(this,ordered);
	}
	//Update stats
	lost          = losts.GetTotal();
	minWaitedTime = packets.GetMinWaitedime();
	maxWaitedTime = packets.GetMaxWaitedTime();
	avgWaitedTime = packets.GetAvgWaitedTime();
}

void RTPIncomingSourceGroup::Stop()
{
	//Stop timer
	dispatchTimer->Cancel();
	
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
	
	//Set extendedd  timestamp
	packet->SetTimestampCycles(source->ExtendTimestamp(packet->GetTimestamp()));
	//Set cycles back
	packet->SetSeqCycles(source->ExtendSeqNum(packet->GetSeqNum()));
	
	//Parse dependency structure now
	if (packet->ParseDependencyDescriptor(templateDependencyStructure,activeDecodeTargets))
	{
		//If it has a new dependency structure
		if (packet->HasTemplateDependencyStructure())
		{
			//Store it
			templateDependencyStructure = packet->GetTemplateDependencyStructure();
			activeDecodeTargets = packet->GetActiveDecodeTargets();
		}
	}
	
	//Set clockrate
	source->clockrate = packet->GetClockRate();

	//if it is video
	if (type == MediaFrame::Video)
	{
		//Check if we can ge the layer info
		auto info = VideoLayerSelector::GetLayerIds(packet);
		//UltraDebug("-VideoLayerSelector::GetLayerIds() | [id:%x,tid:%u,sid:%u]\n",info.GetId(),info.temporalLayerId,info.spatialLayerId);
		//Lock sources accumulators
		ScopedLock scoped(*source);
		//Update source and layer info
		source->Update(time, packet->GetSeqNum(), packet->GetRTPHeader().GetSize() + packet->GetMediaLength(), info);
	} else {
		//Lock sources accumulators
		ScopedLock scoped(*source);
		//Update source and layer info
		source->Update(time, packet->GetSeqNum(), packet->GetRTPHeader().GetSize() + packet->GetMediaLength());
	}
	
	if (source==&media)
		source->SetLastTimestamp(time, packet->GetExtTimestamp());
	//Done
	return source;
}


void RTPIncomingSourceGroup::onTargetBitrateRequested(DWORD bitrate)
{
	UltraDebug("-RTPIncomingSourceGroup::onTargetBitrateRequested() | [bitrate:%d]\n",bitrate);
	//store estimation
	remoteBitrateEstimation = bitrate;
}
