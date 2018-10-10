#include "rtp/RTPIncomingSourceGroup.h"

#include <math.h>

#include "VideoLayerSelector.h"

RTPIncomingSourceGroup::RTPIncomingSourceGroup(MediaFrame::Type type) 
	: losts(128)
{
	this->rtt = 0;
	this->rttrtxSeq = 0;
	this->rttrtxTime = 0;
	this->type = type;
	//Small initial bufer of 100ms
	packets.SetMaxWaitTime(100);
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

void RTPIncomingSourceGroup::Update()
{
	Update(getTimeMS());
}

void RTPIncomingSourceGroup::Update(QWORD now)
{
	//Lock sources accumulators
	ScopedLock scoped(mutex);
	
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


