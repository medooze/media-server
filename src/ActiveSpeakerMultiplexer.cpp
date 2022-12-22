#include "ActiveSpeakerMultiplexer.h"

#include <algorithm>
#include <log.h>
#include <chrono>

using namespace std::chrono_literals;

static const uint64_t	ScorePerMiliScond = 10;
static const auto	MinInterval = 10ms;

ActiveSpeakerMultiplexer::ActiveSpeakerMultiplexer(TimeService& timeService, Listener* listener) :
	timeService(timeService),
	listener(listener)
{
	//Create processing timer
	timer = timeService.CreateTimer(0ms, MinInterval, [this](auto now) { Process(now.count()); });
	//Set name for debug
	timer->SetName("ActiveSpeakerMultiplexer - process");
}

ActiveSpeakerMultiplexer::~ActiveSpeakerMultiplexer()
{
	//Stop timer
	if (timer) timer->Cancel();
}

void ActiveSpeakerMultiplexer::Stop()
{
	Debug(">ActiveSpeakerMultiplexer::Stop()\n");

	//Stop timer
	timer->Cancel();

	//Stop other stuff sync
	timeService.Sync([=](auto) {
		//TODO:Clean stuff

		//Release incoming sources
		for (const auto& [incoming,source] : sources)
			//Remove listener
			incoming->RemoveListener(this);
		//Clear sources
		sources.clear();
	});

	//No timer
	timer = nullptr;

	Debug("<ActiveSpeakerMultiplexer::Stop()\n");
}

void ActiveSpeakerMultiplexer::AddRTPStreamTransponder(RTPStreamTransponder* transpoder, uint32_t id)
{
	Debug("-ActiveSpeakerMultiplexer::AddRTPStreamTransponder() [transpoder:%p,id:%d]\n", transpoder, id);

	if (!transpoder)
		return;

	timeService.Sync([=](const auto& now) {
		//Insert new 
		destinations.try_emplace(transpoder, Destination{id,transpoder});
	});
}
void ActiveSpeakerMultiplexer::RemoveRTPStreamTransponder(RTPStreamTransponder* transpoder)
{
	Debug("-ActiveSpeakerMultiplexer::RemoveRTPStreamTransponder() [transpoder:%p]\n", transpoder);

	if (!transpoder)
		return;

	timeService.Sync([=](const auto& now) {
		//Remove
		destinations.erase(transpoder);
	});
}

void ActiveSpeakerMultiplexer::AddIncomingSourceGroup(RTPIncomingMediaStream::shared incoming, uint32_t id)
{
	Debug("-ActiveSpeakerMultiplexer::AddIncomingSourceGroup() [incoming:%p,id:%d]\n", incoming, id);

	if (!incoming)
		return;

	timeService.Sync([=](const auto& now){
		//Insert new 
		auto [it, inserted] = sources.try_emplace(incoming.get(), Source{id, incoming});
		//If already present
		if (!inserted)
			//do nothing
			return;
		//Add us as rtp listeners
		incoming->AddListener(this);
	});
}

void ActiveSpeakerMultiplexer::RemoveIncomingSourceGroup(RTPIncomingMediaStream::shared incoming)
{
	Debug("-ActiveSpeakerMultiplexer::RemoveIncomingSourceGroup() [incoming:%p]\n", incoming);

	if (!incoming)
		return;

	timeService.Sync([=](const auto& now) {
		Debug("-ActiveSpeakerMultiplexer::RemoveIncomingSourceGroup() async [incoming:%p]\n", incoming);
		//Find source
		auto it = sources.find(incoming.get());
		//check it was not present
		if (it==sources.end())
		{
			Debug("-ActiveSpeakerMultiplexer::RemoveIncomingSourceGroup() async not found[incoming:%p]\n", incoming);
			//Do nothing, probably called onEnded before
			return;
		}
		//Remove us from listeners
		incoming->RemoveListener(this);
		//Get source id
		auto sourceId = it->second.id;
		//Remove it
		sources.erase(it);
		//For each destination transpoder
		for (auto& [transponder, destination] : destinations)
		{
			//If attached to it
			if (destination.sourceId == sourceId)
			{
				//Remove it
				transponder->SetIncoming(nullptr, nullptr);
				//Event
				listener->onActiveSpeakerRemoved(destination.id);
				//Reset source id and timestamp
				destination.sourceId = 0;
				destination.ts = 0;
				//Log
				//Log
				Debug("-ActiveSpeakerMultiplexer::RemoveIncomingSourceGroup() | onActiveSpeakerRemoved [multiplexId:%d]\n", destination.id);
			}
		}
		
	});
}

void ActiveSpeakerMultiplexer::onRTP(const RTPIncomingMediaStream* stream, const std::vector<RTPPacket::shared>& packets)
{
	//For each packet
	for (const auto& packet : packets)
		//handle rtp packet
		onRTP(stream, packet);
}
void ActiveSpeakerMultiplexer::onRTP(const RTPIncomingMediaStream* incoming, const RTPPacket::shared& packet)
{
	//Log
	//Debug("-ActiveSpeakerMultiplexer::onRTP() [ssrc:%d,seqnum:%u]\n", packet->GetSSRC(), packet->GetSeqNum());

	if (!packet)
		//Exit
		return;

	timeService.Async([=, packet = packet](auto) {
		//Double check we have audio level
		if (!packet->HasAudioLevel())
			return;

		//Get vad level
		auto vad = packet->GetVAD();
		auto db	 = packet->GetLevel();
		auto now = getTimeMS();

		//Check voice is detected and not muted
		auto speaking = vad && db != 127 && (!noiseGatingThreshold || db < noiseGatingThreshold);

		//Debug("-ActiveSpeakerMultiplexer::onRTP() [vad:%d,db:%d,speaking:%d]\n", vad,db,speaking);

		//Accumulate only if audio has been detected 
		if (speaking)
		{
			//Get incoming source
			auto it = sources.find(incoming);
			//check it was present
			if (it == sources.end())
				//Do nothing
				return;

			// The audio level is expressed in -dBov, with values from 0 to 127
			// representing 0 to -127 dBov. dBov is the level, in decibels, relative
			// to the overload point of the system.
			// The audio level for digital silence, for example for a muted audio
			// source, MUST be represented as 127 (-127 dBov).
			WORD level = 64 + (127 - db) / 2;

			//Get time diff from last score, we consider 1s as max to coincide with initial bump
			uint64_t diff = std::min(now - it->second.ts, (uint64_t)1000ul);
		
			//Do not accumulate too much so we can switch faster
			it->second.score = std::min(it->second.score + diff * level / ScorePerMiliScond, maxAcummulatedScore);
			//Set last update time
			it->second.ts = now;
			//Add packets for forwarding in case it is selected for multiplex
			it->second.packets.push_back(packet);

			//Log
			//Debug("-ActiveSpeakerMultiplexer::onRTP() Accumulate [id:%u,vad:%d,dbs:%u,score:%lu]\n",it->second.id,vad,db,it->second.score);
		}
	});
}

void ActiveSpeakerMultiplexer::onBye(const RTPIncomingMediaStream* group)
{
}

void ActiveSpeakerMultiplexer::onEnded(const RTPIncomingMediaStream* incoming)
{
	Debug("-ActiveSpeakerDetectorFacade::onEnded() [incoming:%p]\n", incoming);

	if (!incoming)
		return;

	timeService.Async([=](auto) {

		Debug("-ActiveSpeakerDetectorFacade::onEnded() async [incoming:%p]\n", incoming);

		//Find source
		auto it = sources.find(incoming);
		//check it was not present
		if (it == sources.end())
		{
			Debug("-ActiveSpeakerMultiplexer::onEnded() async not found[incoming:%p]\n", incoming);
			//Do nothing, probably called onEnded before
			return;
		}
		//Get source id
		auto sourceId = it->second.id;
		//Remove it
		sources.erase(it);
		//For each destination transpoder
		for (auto& [transponder, destination] : destinations)
		{
			//If attached to it
			if (destination.sourceId == sourceId)
			{
				//Event
				listener->onActiveSpeakerRemoved(destination.id);
				//Reset source id and timestamp
				destination.sourceId = 0;
				destination.ts = 0;
				//Log
				Debug("-ActiveSpeakerMultiplexer::onEnded() | onActiveSpeakerRemoved [multiplexId:%d]\n", destination.id);
			}
		}
	});
}

void ActiveSpeakerMultiplexer::Process(uint64_t now)
{
	//Get difference from last process
	uint64_t diff = last ? now - last : MinInterval.count();

	//Store last
	last = now;

	//Reduce accumulated voice activity
	uint64_t decay = diff * ScorePerMiliScond;

	std::vector<Source*> candidates;
	std::vector<Source*> top(destinations.size());

	//UltraDebug("-ActiveSpeakerMultiplexer::Process() [now:%llu]\n",now);

	//For each source
	for (auto& entry : sources)
	{
		//Decay
		if (entry.second.score > decay)
			//Decrease score
			entry.second.score -= decay;
		else
			//None
			entry.second.score = 0;

		//UltraDebug("-ActiveSpeakerMultiplexer::Process() | part [id:%u,score:%llu,decay:%llu]\n",entry.first,entry.second.score,decay);

		//Check if it is active speaker
		if (entry.second.score>minActivationScore)
			//Add to potential candidates
			candidates.push_back(&(entry.second));
	}

	//If no candidadtes
	if (candidates.empty())
		//Done
		return;

	//for (auto source : candidates)
	//	if (source)
	//		Debug("-ActiveSpeakerMultiplexer::Process() | candidates [id:%d,score:%d]\n", source->id, source->score);

	//Get top candidates by score
	std::partial_sort_copy(candidates.begin(), candidates.end(), top.begin(), top.end());
	
	//for (auto source : top)
	//	if (source)
	//		Debug("-ActiveSpeakerMultiplexer::Process() | top [id:%dscore:%d]\n", source->id, source->score);

	//The list of available destinations, ordered by last, id
	std::map<std::pair<uint64_t, uint32_t>,Destination*> availables;

	//For each destination transpoder
	for (auto& [transponder, destination] : destinations)
	{
		bool available = true;
		//Get attached incoming stream
		auto incoming = transponder->GetIncoming();
		//If attached
		if (incoming)
		{
			//Check if it is on the top sources
			for (auto it = top.begin(); it!= top.end(); )
			{
				//Get source
				Source* source = *it;
				//If no more sources available
				if (!source)
					//Done
					break;
				//If is the attached source
				if (source->incoming == incoming)
				{
					//Delete the source from the top ones
					it = top.erase(it);
					//This transceiver is already attached to a top source
					available = false;
					//Done
					break;
				}
				//Next
				++it;
			}
		}
		//If aviable
		if (available)
			//Add to the list of available transpoders
			availables.try_emplace({destination.ts,destination.id},&destination);
	}

	//for (auto source : top)
	//	if (source)
	//		Debug("-ActiveSpeakerMultiplexer::Process() | pending top [id:%d,score:%d]\n", source->id, source->score);

	//Assing top sources in descending order
	auto it = top.begin();

	//For each available transceiver
	for (auto& [key,destination] : availables)
	{
		//If no more sources available
		if (it==top.end())
			//Done
			break;
		//Get source
		Source* source = *it;
		//If no more sources available
		if (!source)
			//Done
			break;

		//UltraDebug("-ActiveSpeakerMultiplexer::Process() | transponder available [%d,last:%llu]\n", key.second, key.first);
		
		//Get multiplex id
		const auto multiplexId = destination->id;
		//Get source id
		const auto sourceId = source->id;
		
		//Log
		UltraDebug("-ActiveSpeakerMultiplexer::Process() | onActiveSpeakerChanged [sourceId:%d,multiplexId:%d]\n", sourceId, multiplexId);

		//Attach them
		destination->transponder->SetIncoming(source->incoming,nullptr);
		//Send all pending packets
		for (const auto& packet : source->packets)
			//Send it
			destination->transponder->onRTP(source->incoming.get(), packet);
		//Event
		listener->onActiveSpeakerChanged(sourceId, multiplexId);
		//Set last multiplexed source and timestamp
		destination->sourceId = sourceId;
		destination->ts = now;
		//Use next top source
		++it;
	}

	//Clear all packets from all sources
	for (auto& [incoming,source] : sources)
		//No pending packets
		source.packets.clear();
}
