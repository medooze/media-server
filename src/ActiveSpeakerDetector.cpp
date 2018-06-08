#include "ActiveSpeakerDetector.h"

#include <algorithm>

static const uint64_t ScorePerMiliScond = 10000ul;
static const uint64_t MaxScore = ScorePerMiliScond*6000ul;
static const uint64_t MinInterval = 200;

void ActiveSpeakerDetector::Accumulate(uint32_t id, bool vad, uint8_t level, uint64_t now)
{
	//Search for the speacker
	auto it = speakers.find(id);

	//Process vads and check new 
	Process(now);
	
	//Check if we had that speakcer before
	if (it==speakers.end())
	{
		//Store 1s of initial bump if vad
		speakers[id] = {vad ? ScorePerMiliScond*1000ul : 0ul,now};
	} 
	//Accumulate only if audio has been detected
	else if (vad)
	{
		//Get time diff from last score, we consider 1s as max to coincide with initial bump
		uint64_t diff = std::min(now-it->second.ts,1000ul);
		//Do not accumulate too much so we can switch faster
		it->second.score = std::min(it->second.score+level*diff/ScorePerMiliScond,MaxScore);
		//Set last update time
		it->second.ts = now;
	}
}

void ActiveSpeakerDetector::Release(uint32_t id)
{
	//Remove speaker
	speakers.erase(id);
}

void ActiveSpeakerDetector::Process(uint64_t now)
{
	uint64_t active = 0;
	uint64_t maxScore = 0;
	//Get difference from last process
	auto diff = last-now;
	
	//If we have processed it quite recently
	if (diff<MinInterval)
		return;
	
	//Reduce accumulated voice activity
	auto decay = diff/ScorePerMiliScond;
	
	//For each 
	for (auto& entry : speakers)
	{
		//Decay
		entry.second.score = std::max(entry.second.score-decay,(uint64_t)0ul);
		//Check if it is active speaker
		if (maxScore<entry.second.score)
		{
			//New active
			active = entry.first;
			maxScore = entry.second.score;
		}
	}
	
	//IF active has changed and we are out of the block period
	if (active!=lastActive && now>blockedUntil)
	{
		//Event
		listener->onActiveSpeakerChanded(active);
		//Store last aceive and calculate blocking time
		lastActive = active;
		blockedUntil = now + minChangePeriod;
	}
	
	//Update laste process time
	last = now;
}
