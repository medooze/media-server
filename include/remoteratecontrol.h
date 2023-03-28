/* 
 * File:   remoteratecontrol.h
 * Author: Sergio
 *
 * Created on 26 de diciembre de 2012, 12:49
 */

#ifndef REMOTERATECONTROL_H
#define	REMOTERATECONTROL_H

#include "config.h"
#include "acumulator.h"
#include "EventSource.h"

class RemoteRateControl
{

public:
	enum BandwidthUsage
	{
		UnderUsing = 0,
		Normal = 1,
		OverUsing = 2
	};

	enum Region {
		MaxUnknown,
		AboveMax,
		NearMax,
		BelowMax
	};

	static const char * GetName(BandwidthUsage usage)
	{
		switch (usage)
		{
			case UnderUsing:
				return "UnderUsing";
			case Normal:
				return "Normal";
			case OverUsing:
				return "OverUsing";
		}
		return "Unknown";
	}
	
	static const char * GetName(Region region)
	{
		switch (region)
		{
			case MaxUnknown:
				return "MaxUnknown";
			case AboveMax:
				return "AboveMax";
			case NearMax:
				return "NearMax";
			case BelowMax:
				return "BelowMax";
		}
		return "Unknown";
	}
public:
	RemoteRateControl();
	void Update(QWORD time,QWORD ts,DWORD size, bool mark);
	bool UpdateRTT(DWORD rtt);
	bool UpdateLost(DWORD num);
	void SetRateControlRegion(Region region);
	BandwidthUsage GetUsage()	{ return hypothesis; }
	double GetNoise()		{ return varNoise;   }
	void SetEventSource(EvenSource *eventSource) {	this->eventSource = eventSource; }

private:
	void UpdateKalman(int deltaTime, int deltaSize);
private:
	EvenSource *eventSource;
	MinMaxAcumulator<uint32_t, uint64_t> bitrateCalc;
	MinMaxAcumulator<uint32_t, uint64_t> fpsCalc;
	MinMaxAcumulator<uint32_t, uint64_t> packetCalc;
	MinMaxAcumulator<uint32_t, uint64_t> lostCalc;
	DWORD rtt;
	DWORD absSendTimeCycles;
	QWORD prevTS;
	QWORD prevTime;
	DWORD prevSize;
	QWORD curTS;
	QWORD curTime;
	DWORD curSize;
	DWORD prevTarget;
	int64_t curDelta;
	int64_t prevDelta;
	double slope;
	double offset;
	double E[2][2];
	double processNoise[2];
	double avgNoise;
	double varNoise;
	double threshold;
	double prevOffset;
	BandwidthUsage hypothesis;
	int overUseCount;
};

#endif	/* REMOTERATECONTROL_H */

