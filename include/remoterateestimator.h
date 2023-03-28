/* 
 * File:   remoterateestimator.h
 * Author: Sergio
 *
 * Created on 8 de marzo de 2013, 10:43
 */

#ifndef REMOTERATEESTIMATOR_H
#define	REMOTERATEESTIMATOR_H

#include "use.h"
#include "remoteratecontrol.h"
#include "rtp/RTPPacket.h"

class RemoteRateEstimator
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onTargetBitrateRequested(DWORD bitrate, DWORD bandwidthEstimation, DWORD totalBitrate) = 0;
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
	RemoteRateEstimator();
	~RemoteRateEstimator();
	void SetListener(Listener *listener);
	void AddStream(DWORD ssrc);
	void RemoveStream(DWORD ssrc);
	void UpdateRTT(DWORD ssrc,DWORD rtt, QWORD now);
	void UpdateLost(DWORD ssrc,DWORD lost, QWORD now);
	void Update(DWORD ssrc,const RTPPacket::shared& packet,DWORD size);
	void Update(DWORD ssrc,QWORD now,QWORD ts,DWORD size, bool mark);
	DWORD GetEstimatedBitrate();
	void GetSSRCs(std::list<DWORD> &ssrcs);
	void SetTemporalMaxLimit(DWORD limit);
	void SetTemporalMinLimit(DWORD limit);
	void SetEventSource(EvenSource *eventSource) {	this->eventSource = eventSource; }
private:
	double RateIncreaseFactor(QWORD now, QWORD last, DWORD reactionTime) const;
	void Update(RemoteRateControl::BandwidthUsage usage,bool reactNow,QWORD now);
	void UpdateChangePeriod(QWORD now);
	void UpdateMaxBitRateEstimate(float incomingBitRateKbps);
	void ChangeState(State newState);
	void ChangeRegion(RemoteRateControl::Region newRegion);
private:
	typedef std::map<DWORD,RemoteRateControl*> Streams;
private:
	Listener*	listener    = NULL;
	EvenSource*	eventSource = NULL;
	MinMaxAcumulator<uint32_t, uint64_t>	bitrateAcu;
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
	DWORD noiseVar;
	QWORD curTS;
	DWORD absSendTimeCycles;
	float avgChangePeriod;
	QWORD lastChange;
	float beta;
	DWORD rtt;
};

#endif	/* REMOTERATEESTIMATOR_H */

