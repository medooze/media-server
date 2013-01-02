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




class RemoteRateControl
{
public:
	class Listener
	{
	public:
		virtual void onTargetBitrateRequested(DWORD bitrate) = 0;
	};
public:
	enum BandwidthUsage
	{
		Normal,
		OverUsing,
		UnderUsing
	};
public:
	RemoteRateControl(Listener* listener);
	void Update(RTPTimedPacket* packet);
	void UpdateRTT(DWORD rtt);
private:
	void UpdateKalman(QWORD now,QWORD t_delta, double ts_delta, DWORD frame_size, DWORD prev_frame_size);
private:
	Listener*  listener;
	Acumulator bitrateCalc;
	DWORD rtt;

	WORD num;
	DWORD prevTS;
	QWORD prevTime;
	DWORD prevSize;
	DWORD curSize;
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

