/* 
 * File:   remoterateestimator.h
 * Author: Sergio
 *
 * Created on 8 de marzo de 2013, 10:43
 */

#ifndef REMOTERATEESTIMATOR_H
#define	REMOTERATEESTIMATOR_H

#include "remoteratecontrol.h"
#include "use.h"


class RemoteRateEstimator
{
public:
	enum State {
		Hold,
		Increase,
		Decrease
	};

	const char * GetName(State state)
	{
		switch (state)
		{
			case Hold:
				return "Hold";
			case Increase:
				return "Increase";
			case Decrease:
				return "Decrease";
		}
		return "Unknown";
	}
public:
	RemoteRateEstimator();
	void AddStream(DWORD ssrc,RemoteRateControl* ctrl);
	void RemoveStream(DWORD ssrc);
	void SetRTT(DWORD rtt);
	void Update(DWORD size);
	DWORD GetEstimatedBitrate();
	void GetSSRCs(std::list<DWORD> &ssrcs);
private:
	double RateIncreaseFactor(QWORD nowMs, QWORD lastMs, DWORD reactionTimeMs, double noiseVar) const;
	void UpdateChangePeriod(QWORD nowMs);
	void UpdateMaxBitRateEstimate(float incomingBitRateKbps);
	void ChangeState(State newState);
	void ChangeRegion(RemoteRateControl::Region newRegion);
private:
	typedef std::map<DWORD,RemoteRateControl*> Streams;
private:
	Acumulator	bitrateAcu;
	Streams		streams;
	Use		lock;
	DWORD minConfiguredBitRate;
	DWORD maxConfiguredBitRate;
	DWORD currentBitRate;
	DWORD maxHoldRate;
	float avgMaxBitRate;
	float varMaxBitRate;
	State state;
	State cameFromState;
	RemoteRateControl::Region region;
	QWORD lastBitRateChange;

	float avgChangePeriod;
	QWORD lastChangeMs;
	float beta;
	DWORD rtt;

};

#endif	/* REMOTERATEESTIMATOR_H */

