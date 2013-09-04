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
	class Listener
	{
	public:
		virtual void onTargetBitrateRequested(DWORD bitrate) = 0;
	};
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
	RemoteRateEstimator(const std::wstring& tag);
	void SetListener(Listener *listener);
	void AddStream(DWORD ssrc);
	void RemoveStream(DWORD ssrc);
	void UpdateRTT(DWORD ssrc,DWORD rtt);
	void UpdateLost(DWORD ssrc,DWORD lost);
	void Update(DWORD ssrc,RTPTimedPacket* packet);
	DWORD GetEstimatedBitrate();
	void GetSSRCs(std::list<DWORD> &ssrcs);
	void SetTemporalMaxLimit(DWORD limit);
	void SetTemporalMinLimit(DWORD limit);
private:
	double RateIncreaseFactor(QWORD nowMs, QWORD lastMs, DWORD reactionTimeMs, double noiseVar) const;
	void UpdateChangePeriod(QWORD nowMs);
	void UpdateMaxBitRateEstimate(float incomingBitRateKbps);
	void ChangeState(State newState);
	void ChangeRegion(RemoteRateControl::Region newRegion);
private:
	typedef std::map<DWORD,RemoteRateControl*> Streams;
private:
	Listener*	listener;
	EvenSource	eventSource;
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

