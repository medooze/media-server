#include "rtp/RTPIncomingSourceGroup.h"

#include <math.h>

#include "VideoLayerSelector.h"
#include "remoterateestimator.h"

using namespace std::chrono_literals;

RTPIncomingSourceGroup::RTPIncomingSourceGroup(MediaFrame::Type type,TimeService& timeService) :
	timeService(timeService),
	losts(1024)
{
	//Store type
	this->type = type;
	//Small initial bufer of 1000s
	packets.SetMaxWaitTime(1000);
	//LIsten remote rate events
	remoteRateEstimator.SetListener(this);
	//Create dispatch timer
	dispatchTimer = timeService.CreateTimer([this](auto now){ DispatchPackets(now.count()); });
	//Set name for debug
	dispatchTimer->SetName("RTPIncomingSourceGroup - dispatch");
}

RTPIncomingSource* RTPIncomingSourceGroup::GetSource(DWORD ssrc)
{
	if (ssrc == media.ssrc)
		return &media;
	else if (ssrc == rtx.ssrc)
		return &rtx;
	return NULL;
}

void RTPIncomingSourceGroup::AddListener(RTPIncomingMediaStream::Listener* listener) 
{
	Debug("-RTPIncomingSourceGroup::AddListener() [listener:%p]\n",listener);
	
	//Add it sync
	timeService.Sync([=](auto){
		listeners.insert(listener);
	});
}

void RTPIncomingSourceGroup::RemoveListener(RTPIncomingMediaStream::Listener* listener) 
{
	Debug("-RTPIncomingSourceGroup::RemoveListener() [listener:%p]\n",listener);
	
	//Remove it sync
	timeService.Sync([=](auto) {
		listeners.erase(listener);
	});
}

int RTPIncomingSourceGroup::AddPacket(const RTPPacket::shared &packet, DWORD size, QWORD now)
{
	
	//UltraDebug(">RTPIncomingSourceGroup::AddPacket() | [now:%lld]\n",now);
	
	//Check if it is the rtx packet used to calculate rtt
	if (rttrtxTime && packet->GetSeqNum()==rttrtxSeq)
	{
		//Get packet time
		const auto time = packet->GetTime();
		//Calculate rtt
		const auto rtt = time - rttrtxTime;
		//Set RTT
		SetRTT(rtt,now);
		//Done
		return 0;
	}
	
	//If it is not from the media ssrc
	if (packet->GetSSRC()!=media.ssrc)
		//Do nothing else
		return 0;
	
	//Add to lost packets
	auto lost = losts.AddPacket(packet);
	
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
			//Add it sync
			timeService.Sync([=](auto) {
				//Deliver to all listeners
				for (auto listener : listeners)
					//Dispatch rtp packet
					listener->onBye(this);
			});
		}
	} else if (ssrc == rtx.ssrc) {
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
	//Update it sync
	timeService.Sync([=](std::chrono::milliseconds now) {
		//Update
		media.Update(now.count());
		//Update
		rtx.Update(now.count());
	});
}

void RTPIncomingSourceGroup::Update(QWORD now)
{
	//Update it sync
	timeService.Sync([=](auto) {
		//Update
		media.Update(now);
		//Update
		rtx.Update(now);
	});
}

void RTPIncomingSourceGroup::SetRTT(DWORD rtt, QWORD now)
{
	//Store rtt
	this->rtt = rtt;
	//if se suport rtx
	if (isRTXEnabled)
		//Set max packet wait time
		packets.SetMaxWaitTime(fmin(750,fmax(200,rtt)*3));
	else
		//No wait
		packets.SetMaxWaitTime(0);
	//Dispatch packets with new timer now
	dispatchTimer->Again(0ms);
	//If using remote rate estimator
	if (remb)
		//Add estimation
		remoteRateEstimator.UpdateRTT(media.ssrc,rtt,now);
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
	for (auto packet = packets.GetOrdered(time); packet; packet = packets.GetOrdered(time))
	{
		//We need to adjust the seq num due the in band probing packets
		packet->SetExtSeqNum(packet->GetExtSeqNum() - packets.GetNumDiscardedPackets());
		//Add to packets
		ordered.push_back(packet);
	}
	
	//Deliver to all listeners
	for (auto listener : listeners)
		//Dispatch rtp packet
		listener->onRTP(this,ordered);

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
	
	//Stop listeners sync
	timeService.Sync([=](auto) {
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onEnded(this);
		//Clear listeners
		listeners.clear();
	});
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
		//Update source and layer info
		source->Update(time, packet->GetSeqNum(), packet->GetRTPHeader().GetSize() + packet->GetMediaLength(), info, VideoLayerSelector::AreLayersInfoeAggregated(packet));
	} else {
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

void RTPIncomingSourceGroup::SetRTXEnabled(bool enabled)
{
	UltraDebug("-RTPIncomingSourceGroup::EnableRTX() | [enabled:%d]\n", enabled);
	//store estimation
	isRTXEnabled = enabled;
}