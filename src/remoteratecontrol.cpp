/* 
 * File:   remoteratecontrol.cpp
 * Author: Sergio
 * 
 * Created on 26 de diciembre de 2012, 12:49
 */

#include "remoteratecontrol.h"
#include <math.h>


RemoteRateControl::RemoteRateControl(Listener* listener) : bitrateCalc(1000)
{
	this->listener = listener;
	prevTS = 0;
	prevTime = 0;
	prevSize = 0;
	curSize = 0;
	num = 0;
	slope = 8.0/512.0;
	E[0][0] = 100;
	E[0][1] = 0;
	E[1][0] = 0;
	E[1][1] = 1e-1;
	processNoise[0] = 1e-10;
	processNoise[1] = 1e-2;
	avgNoise = 0;
	varNoise = 50;
	threshold = 25;
	prevOffset = 0;
	offset = 0;
	hypothesis = Normal;
	overUseCount=0;
}

void RemoteRateControl::Update(RTPTimedPacket* packet)
{
	//Get packet size and time
	QWORD time = packet->GetTime();
	DWORD size = packet->GetMediaLength();
	//Update bitrate calculator and get instant bitrate
	QWORD bitrate = bitrateCalc.Update(time, size*8);
	//Get current seq num
	DWORD seq = packet->GetSeqNum();
	//Get rtp timestamp in ms
	DWORD ts = packet->GetClockTimestamp();
	//If it is a our of order packet from previous frame
	if (ts < prevTS)
		//Exit
		return;

	//Check if it is from a new frame
	if (ts > prevTS)
	{
		//If not first one
		if (prevTime)
			//Update Kalman filter as per google algorithm
			UpdateKalman(time - prevTime, ts - prevTS, curSize, prevSize);
		//Move
		prevTS = ts;
		prevTime = time;
		prevSize = curSize;
		curSize = 0;
	}
	//Update size of current frame
	curSize += size;
}

void RemoteRateControl::UpdateKalman(QWORD tdelta, double tsdelta, DWORD framesize, DWORD prevframesize)
{
	if (num<60)
		num++;
	else
		num = 60;

	const double ttsdelta = tdelta - tsdelta;
	double fsdelta = static_cast<double> (framesize) - prevframesize;

	// Update the Kalman filter
	E[0][0] += processNoise[0];
	E[1][1] += processNoise[1];

	if ((hypothesis == OverUsing && offset < prevOffset) || 
	    (hypothesis == UnderUsing && offset > prevOffset)   )
		E[1][1] += 10 * processNoise[1];

	const double h[2] = 
	{	fsdelta,
		1.0
	};
	const double Eh[2] =
	{
		E[0][0] * h[0] + E[0][1] * h[1],
		E[1][0] * h[0] + E[1][1] * h[1]
	};

	const double residual = ttsdelta - slope * h[0] - offset;

	// Only update the noise estimate if we're not over-using
	if (num * fabsf(offset) < threshold)
	{
		// Faster filter during startup to faster adapt to the jitter level
		// of the network alpha is tuned for 30 frames per second, but
		double alpha = 0.01;
		if (num == 60)
			alpha = 0.002;

		// beta is a function of alpha and the time delta since
		// the previous update.
		const double beta = pow(1 - alpha, tsdelta * 30.0 / 1000.0);
		avgNoise = beta * avgNoise + (1 - beta) * residual;
		varNoise = beta * varNoise + (1 - beta) * (avgNoise - residual) * (avgNoise - residual);
	}

	const double denom = varNoise + h[0] * Eh[0] + h[1] * Eh[1];
	const double K[2] = 
	{
		Eh[0] / denom,
		Eh[1] / denom
	};
	const double IKh[2][2] =
	{
		{ 1.0 - K[0] * h[0] ,     - K[0] * h[1] },
		{     - K[1] * h[0] , 1.0 - K[1] * h[1] }
	};

	// Update state
	E[0][0] = E[0][0] * IKh[0][0] + E[1][0] * IKh[0][1];
	E[0][1] = E[0][1] * IKh[0][0] + E[1][1] * IKh[0][1];
	E[1][0] = E[0][0] * IKh[1][0] + E[1][0] * IKh[1][1];
	E[1][1] = E[0][1] * IKh[1][0] + E[1][1] * IKh[1][1];

	slope = slope + K[0] * residual;
	prevOffset = offset;
	offset = offset + K[1] * residual;

	const double T = num * offset;

	DWORD target = 0;

	//Compare
	if (fabsf(T)>threshold)
	{
		///Detect overuse
		if (offset>0)
		{
			//LOg
			if (hypothesis!=OverUsing )
			{
				//Check 
				if (overUseCount>2)
				{
					//Go conservative
					target = bitrateCalc.GetInstant()*1000/bitrateCalc.GetWindow();
					//Log
					Log("BWE:  OverUsing bitrate:%lld max:%lld min:%lld target:%d\n",bitrateCalc.GetInstant(),bitrateCalc.GetMax(),bitrateCalc.GetMin(),target);
					//Reset
					bitrateCalc.Reset();
					//Overusing
					hypothesis = OverUsing;
					//Reset counter
					overUseCount=0;
				} else {
					//increase counter
					overUseCount++;
				}
			}
		} else {
			//If we change state
			if (hypothesis!=UnderUsing)
			{
				Log("BWE:  UnderUsing bitrate:%lld max:%lld min:%lld\n",bitrateCalc.GetInstant(),bitrateCalc.GetMax(),bitrateCalc.GetMin());
				//Reset bitrate
				bitrateCalc.Reset();
				//Under using, do nothing until going back to normal
				hypothesis = UnderUsing;
			}
		}
	} else {
		//Check state
		switch(hypothesis)
		{
			case OverUsing:
				//The over use period has been very small, try to keep rate as it seems safe
				target = bitrateCalc.GetInstant()*900/bitrateCalc.GetWindow();
				//Log
				Log("BWE:  Normal  bitrate:%lld max:%lld min:%lld target:%d\n",bitrateCalc.GetInstant(),bitrateCalc.GetMax(),bitrateCalc.GetMin(),target);
				//Reset
				bitrateCalc.Reset();
				//Going to normal
				hypothesis = Normal;
				break;
			case UnderUsing:
				//Get maximum bitrate during the under use period, it is safe
				target = fmax(bitrateCalc.GetMax()*1200/bitrateCalc.GetWindow(),bitrateCalc.GetInstant()*1200/bitrateCalc.GetWindow());
				//Log
				Log("BWE:  Normal  bitrate:%lld max:%lld min:%lld target:%d\n",bitrateCalc.GetInstant(),bitrateCalc.GetMax(),bitrateCalc.GetMin(),target);
				//Reset
				bitrateCalc.Reset();
				break;
			case Normal:
				//We should periodically increase the bitrate if conditions are good
				//Check
				break;
		}
		//Normal
		hypothesis = Normal;
	}

	//If we have a new target bitrate
	if (target && listener)
		//Call listenter
		listener->onTargetBitrateRequested(target);
}

void RemoteRateControl::UpdateRTT(DWORD rtt)
{
	this->rtt = rtt;
}
