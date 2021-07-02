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
	timer = timeService.CreateTimer(MinInterval, [this](auto now) { Process(now.count()); });
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
}

void ActiveSpeakerMultiplexer::AddRTPStreamTransponder(RTPStreamTransponder* transpoder, uint32_t id)
{
	Debug("-ActiveSpeakerMultiplexer::AddRTPStreamTransponder() [transpoder:%p,id:%d]\n", transpoder, id);

	if (!transpoder)
		return;

	timeService.Sync([=](const auto& now) {
		//Insert new 
		transponders.try_emplace(transpoder, id);
	});
}
void ActiveSpeakerMultiplexer::RemoveRTPStreamTransponder(RTPStreamTransponder* transpoder)
{
	Debug("-ActiveSpeakerMultiplexer::RemoveRTPStreamTransponder() [transpoder:%p]\n", transpoder);

	if (!transpoder)
		return;

	timeService.Sync([=](const auto& now) {
		//Remove
		transponders.erase(transpoder);
	});
}

void ActiveSpeakerMultiplexer::AddIncomingSourceGroup(RTPIncomingMediaStream* incoming, uint32_t id)
{
	Debug("-ActiveSpeakerMultiplexer::AddIncomingSourceGroup() [incoming:%p,id:%d]\n", incoming, id);

	if (!incoming)
		return;

	timeService.Sync([=](const auto& now){
		//Insert new 
		auto [it, inserted] = sources.try_emplace(incoming, Source{id, incoming});
		//If already present
		if (!inserted)
			//do nothing
			return;
		//Add us as rtp listeners
		incoming->AddListener(this);
	});
}

void ActiveSpeakerMultiplexer::RemoveIncomingSourceGroup(RTPIncomingMediaStream* incoming)
{
	Debug("-ActiveSpeakerMultiplexer::RemoveIncomingSourceGroup() [incoming:%p]\n", incoming);

	if (!incoming)
		return;

	timeService.Sync([=](const auto& now) {
		//check it was not present
		if (sources.erase(incoming))
			//Do nothing, probably called onEnded before
			return;
		//Remove listener
		incoming->RemoveListener(this);
	});
}

void ActiveSpeakerMultiplexer::onRTP(RTPIncomingMediaStream* stream, const std::vector<RTPPacket::shared>& packets)
{
	//For each packet
	for (const auto& packet : packets)
		//handle rtp packet
		onRTP(stream, packet);
}
void ActiveSpeakerMultiplexer::onRTP(RTPIncomingMediaStream* incoming, const RTPPacket::shared& packet)
{
	//Double check we have audio level
	if (!packet->HasAudioLevel())
		return;

	//Get vad level
	auto vad = packet->GetVAD();
	auto db	 = packet->GetLevel();
	auto now = getTimeMS();

	//Check voice is detected and not muted
	auto speaking = vad && db != 127 && (!noiseGatingThreshold || db < noiseGatingThreshold);

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
		//UltraDebug("-ActiveSpeakerDetector::Accumulate [id:%u,vad:%d,speaking:%d,diff:%u,level:%u,score:%lu]\n",id,vad,speaking,diff,level,speakers[id].score);
		//Do not accumulate too much so we can switch faster
		it->second.score = std::min(it->second.score + diff * level / ScorePerMiliScond, maxAcummulatedScore);
		//Set last update time
		it->second.ts = now;
		//Add packets for forwarding in case it is selected for multiplex
		it->second.packets.push_back(packet);

		//UltraDebug("-ActiveSpeakerMultiplexer::onRTP() Accumulate [id:%u,vad:%d,dbs:%u,score:%lu]\n",it->second.id,vad,db,it->second.score);
	}
}

void ActiveSpeakerMultiplexer::onBye(RTPIncomingMediaStream* group)
{
}

void ActiveSpeakerMultiplexer::onEnded(RTPIncomingMediaStream* incoming)
{
	Debug("-ActiveSpeakerDetectorFacade::onEnded() [incoming:%p]\n", incoming);

	if (!incoming)
		return;

	//Delete from sources
	sources.erase(incoming);
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
	std::vector<Source*> top(transponders.size());

	//For each source
	for (auto& entry : sources)
	{
		//UltraDebug(">ActiveSpeakerMultiplexer::Process() | part [id:%u,score:%llu,decay:%llu]\n",entry.first,entry.second.score,decay);

		//Decay
		if (entry.second.score > decay)
			//Decrease score
			entry.second.score -= decay;
		else
			//None
			entry.second.score = 0;

		//UltraDebug("<ActiveSpeakerMultiplexer::Process() | part [id:%u,score:%llu,decay:%llu]\n",entry.first,entry.second.score,decay);

		//Check if it is active speaker
		if (entry.second.score>minActivationScore)
			//Add to potential candidates
			candidates.push_back(&entry.second);
	}

	//Get top candidates by score
	auto last = std::partial_sort_copy(candidates.begin(), candidates.end(), top.begin(), top.end());
	
	//The list of available transpoders
	std::vector<RTPStreamTransponder*> availables;

	//For each transpoder
	for (const auto& [transponder, id] : transponders)
	{
		bool available = true;
		//Get attached incoming stream
		auto incoming = transponder->GetIncoming();
		//If attached
		if (incoming)
		{
			//Check if it is on the top sources
			for (auto it = top.begin(); it!= last; )
			{
				//Get source
				Source* source = *it;
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
			availables.push_back(transponder);
	}
	//Assing top sources in descending order
	auto it = top.begin();

	//For each available transceiver
	for (const auto& transponder : availables)
	{
		//If no sources available
		if (it==last)
			//Done
			break;
		//Get source
		Source* source = *it;
		//Get multiplex id
		const auto multiplexId = transponders[transponder];
		//Get source id
		const auto speakerId = source->id;
		//Attach them
		transponder->SetIncoming(source->incoming,nullptr);
		//Send all pending packets
		for (const auto& packet : source->packets)
			//Send it
			transponder->onRTP(source->incoming, packet);
		//Event
		listener->onActiveSpeakerChanded(speakerId, multiplexId);
		//Use next top source
		++it;
	}

	//Clear all packets from all sources
	for (auto& [incoming,source] : sources)
		//No pending packets
		source.packets.clear();
}
