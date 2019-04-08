#ifndef ACTIVESPEAKERDETECTOR_H
#define ACTIVESPEAKERDETECTOR_H
#include "config.h"

class ActiveSpeakerDetector
{
public:
	class Listener
	{
	public:
		virtual void onActiveSpeakerChanded(uint32_t id) = 0;
	};
public:
	ActiveSpeakerDetector(Listener *listener) : listener(listener) {}
	virtual ~ActiveSpeakerDetector() = default;
	void Accumulate(uint32_t id, bool vad, uint8_t db, uint64_t now);
	void Release(uint32_t id);
	void SetMinChangePeriod(uint32_t minChangePeriod)		{ this->minChangePeriod = minChangePeriod;		}
	void SetMaxAccumulatedScore(uint64_t maxAcummulatedScore)	{ this->maxAcummulatedScore = maxAcummulatedScore;	}	
	void SetNoiseGatingThreshold(uint8_t noiseGatingThreshold)	{ this->noiseGatingThreshold = noiseGatingThreshold;	}	
	void SetMinActivationScore(uint32_t minActivationScore)		{ this->minActivationScore = minActivationScore;	}	
protected:
	void Process(uint64_t now);
	
private:
	struct SpeakerInfo
	{
		uint64_t score;
		uint64_t ts;
	};
private:
	uint64_t last			= 0;
	uint64_t blockedUntil		= 0;
	uint32_t minChangePeriod	= 2000; 
	uint32_t lastActive		= 0;
	uint64_t maxAcummulatedScore	= 2500;
	uint8_t noiseGatingThreshold	= 127;
	uint64_t minActivationScore	= 0;
	
	Listener* listener;
	std::map<uint32_t,SpeakerInfo> speakers;
};

#endif /* ACTIVESPEAKERDETECTOR_H */

