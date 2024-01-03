#include "tracing.h"

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
}

RTPIncomingSourceGroup::~RTPIncomingSourceGroup()
{
	//If not stoped
	if (dispatchTimer)
		//Stop
		Stop();
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
	timeService.Async([=](auto){
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
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingSourceGroup::AddPacket");


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

	// Note: This may truncate UNKNOWN but we do that many places elsewhere treating -1 == 0xFF == UNKNOWN so being consistent here as well
	codec = packet->GetCodec();

	//Add to packet queue
	if (!packets.Add(packet))
		//Rejected packet
		return -1;
	
	//Check if we have receiver already an SR for media stream
	if (media.lastReceivedSenderReport)
	{
		//Get ntp and rtp timestamps
		QWORD timestamp = media.lastReceivedSenderRTPTimestampExtender.GetExtSeqNum();
		//Get ts delta		
		int64_t delta = (int64_t)packet->GetExtTimestamp() - timestamp;
		//Calculate sender time 
		QWORD senderTime = media.lastReceivedSenderTime + (delta) * 1000 / packet->GetClockRate();
		//Set calculated sender time
		packet->SetSenderTime(senderTime);
		//Set skew in timestamp unit
		packet->SetTimestampSkew(media.skew * packet->GetClockRate() / 1000);
	}

	//Get next time for dispatcht
	uint64_t next = packets.GetWaitTime(now);
	
	//UltraDebug("-RTPIncomingSourceGroup::AddPacket() | [lost:%d,next:%llu]\n",lost,next);
	
	//If we have anything on the queue
	if (next!=(QWORD)-1 && dispatchTimer)
		//Reschedule timer
		dispatchTimer->Again(std::chrono::milliseconds(next));
	
	//Set last updated time
	lastUpdated = now;

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
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingSourceGroup::Update");

	//Update it sync
	timeService.Sync([=](std::chrono::milliseconds now) {
		//Set last updated time
		lastUpdated = now.count();
		//Update
		media.Update(now.count());
		//Update
		rtx.Update(now.count());
	});
}

void RTPIncomingSourceGroup::UpdateAsync(std::function<void(std::chrono::milliseconds)> callback)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingSourceGroup::Update");

	//Update it sync
	timeService.Async([=](std::chrono::milliseconds now) {
		//Set last updated time
		lastUpdated = now.count();
		//Update
		media.Update(now.count());
		//Update
		rtx.Update(now.count());
	}, callback);
}

void RTPIncomingSourceGroup::SetMaxWaitTime(DWORD maxWaitingTime)
{
	//Update it sync
	timeService.Async([=](std::chrono::milliseconds now) {
		//Set it
		packets.SetMaxWaitTime(maxWaitingTime);
		//Store overriden value
		this->maxWaitingTime = maxWaitingTime;
	});
}

void RTPIncomingSourceGroup::ResetMaxWaitTime()
{
	//Update it sync
	timeService.Async([=](std::chrono::milliseconds now) {
		//Remove override
		maxWaitingTime.reset();
	});
}

void RTPIncomingSourceGroup::SetRTT(DWORD rtt, QWORD now)
{
	//Store rtt
	this->rtt = rtt;
	//If the max wait time is not overriden
	if (!maxWaitingTime.has_value())
	{
		//if se suport rtx
		if (isRTXEnabled)
			//Set max packet wait time
			packets.SetMaxWaitTime(fmin(750,fmax(200,rtt)*3));
		else
			//No wait
			packets.SetMaxWaitTime(0);
	}
	//Dispatch packets with new timer now
	if (dispatchTimer) dispatchTimer->Again(0ms);
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

	//Create dispatch timer
	dispatchTimer = timeService.CreateTimer([this](auto now) { DispatchPackets(now.count()); });
	//Set name for debug
	dispatchTimer->SetName("RTPIncomingSourceGroup - dispatch");
	
	//are we using remb?
	this->remb = remb;
	
	//Add media ssrc
	if (media.ssrc) remoteRateEstimator.AddStream(media.ssrc);
}

void RTPIncomingSourceGroup::DispatchPackets(uint64_t time)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingSourceGroup::DispatchPackets",
		"queued", packets.Length(),
		"next", packets.GetNextPacketSeqNumber(),
		"discarded", packets.GetNumDiscardedPackets(),
		"listeners", listeners.size()
	);

	//UltraDebug("-RTPIncomingSourceGroup::DispatchPackets() | [time:%llu]\n",time);
	
	//Deliver all pending packets at once
	std::vector<RTPPacket::shared> ordered;
	for (auto packet = packets.GetOrdered(time); packet; packet = packets.GetOrdered(time))
	{
		//We need to adjust the seq num due the in band probing packets
		packet->SetExtSeqNum(packet->GetExtSeqNum() - packets.GetNumDiscardedPackets());
		//Add to packets
		ordered.emplace_back(std::move(packet));
	}
	
	//If we have any rtp packets and we are not muted
	if (ordered.size() && !muted)
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onRTP(this,ordered);

	//Update stats
	lost		= losts.GetTotal();
	avgWaitedTime	= packets.GetAvgWaitedTime();
	//Get min max values
	auto minmax	= packets.GetMinMaxWaitedTime();
	minWaitedTime	= minmax.first;
	maxWaitedTime	= minmax.second;

	TRACE_EVENT("rtp", "RTPIncomingSourceGroup::DispatchedPackets",
		"queued", packets.Length(),
		"next", packets.GetNextPacketSeqNumber(),
		"discarded", packets.GetNumDiscardedPackets(),
		"lost", losts.GetTotal(),
		"dispatched", ordered.size(),
		"listeners", listeners.size()
	);

}

void RTPIncomingSourceGroup::Stop()
{

	Debug("-RTPIncomingSourceGroup::Stop()\r\n");

	//Stop timer
	if (dispatchTimer) dispatchTimer->Cancel();

	//Stop listeners sync
	timeService.Sync([=](auto) {
		//Deliver to all listeners
		for (auto listener : listeners)
			//Dispatch rtp packet
			listener->onEnded(this);
		//Clear listeners
		listeners.clear();
	});

	//No timer
	dispatchTimer = nullptr;
}

RTPIncomingSource* RTPIncomingSourceGroup::Process(RTPPacket::shared &packet)
{
	//Trace method
	TRACE_EVENT("rtp", "RTPIncomingSourceGroup::Process");

	//Get packet time
	uint64_t time = packet->GetTime();
	
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
	
	//if it is the main video source
	if (type == MediaFrame::Video && source==&media)
	{
		//Check if we can ge the layer info
		auto info = VideoLayerSelector::GetLayerIds(packet);
		//UltraDebug("-RTPIncomingSourceGroup::Process() | [id:%x,tid:%u,sid:%u]\n",info.GetId(),info.temporalLayerId,info.spatialLayerId);
		//Update source and layer info
		source->Update(time, packet->GetSeqNum(), packet->GetMediaLength(), packet->GetRTPHeader().GetSize(), info, VideoLayerSelector::AreLayersInfoeAggregated(packet), packet->GetVideoLayersAllocation());
		
		//If we have new size
		if (packet->GetWidth() && packet->GetHeight())
		{		
			UltraDebug("-RTPIncomingSourceGroup::Processs() | [width:%u,height:%u]\n", packet->GetWidth(), packet->GetHeight());
			//Update size
			source->width = packet->GetWidth();
			source->height = packet->GetHeight();
		}
	} else {
		//Update source and layer info
		source->Update(time, packet->GetSeqNum(), packet->GetMediaLength(), packet->GetRTPHeader().GetSize());
	}
	
	if (source==&media)
		source->SetLastTimestamp(time, packet->GetExtTimestamp(), packet->GetAbsoluteCaptureTime());
	//Done
	return source;
}


void RTPIncomingSourceGroup::onTargetBitrateRequested(DWORD bitrate, DWORD bandwidthEstimation, DWORD totalBitrate)
{
	UltraDebug("-RTPIncomingSourceGroup::onTargetBitrateRequested() | [bitrate:%d, bandwidthEstimation:%d, totalBitrate:%d]\n", bitrate, bandwidthEstimation, totalBitrate);
	//store estimation
	remoteBitrateEstimation = bitrate;
}

void RTPIncomingSourceGroup::SetRTXEnabled(bool enabled)
{
	UltraDebug("-RTPIncomingSourceGroup::SetRTXEnabled() | [enabled:%d]\n", enabled);
	//store estimation
	isRTXEnabled = enabled;
	//if rtx is not supported
	if (!isRTXEnabled)
		//No wait
		packets.SetMaxWaitTime(0);
}

void RTPIncomingSourceGroup::Mute(bool muting)
{
	//Log
	UltraDebug("-RTPIncomingSourceGroup::Mute() | [muting:%d]\n", muting);

	//Update state
	muted = muting;
}
