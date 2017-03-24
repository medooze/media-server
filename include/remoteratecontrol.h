/* 
 * File:   remoteratecontrol.h
 * Author: Sergio
 *
 * Created on 26 de diciembre de 2012, 12:49
 */

#ifndef REMOTERATECONTROL_H
#define	REMOTERATECONTROL_H

#include "config.h"
#include "rtp.h"
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
	void Update(RTPPacket* packet);
	bool UpdateRTT(DWORD rtt);
	bool UpdateLost(DWORD num);
	void SetRateControlRegion(Region region);
	BandwidthUsage GetUsage()	{ return hypothesis; }
	double GetNoise()		{ return varNoise;   }
	void SetEventSource(EvenSource *eventSource) {	this->eventSource = eventSource; }

private:
	void UpdateKalman(QWORD now,int deltaTime, int deltaTS, int deltaSize);
private:
	EvenSource *eventSource;
	Acumulator bitrateCalc;
	Acumulator fpsCalc;
	Acumulator packetCalc;
	DWORD rtt;
	DWORD absSendTimeCycles;
	DWORD prevTS;
	QWORD prevTime;
	DWORD prevSize;
	DWORD curTS;
	QWORD curTime;
	DWORD curSize;
	DWORD prevTarget;
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

