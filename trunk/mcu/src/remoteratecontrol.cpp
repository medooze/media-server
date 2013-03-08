/* 
 * File:   remoteratecontrol.cpp
 * Author: Sergio
 * 
 * Created on 26 de diciembre de 2012, 12:49
 */

#include "remoteratecontrol.h"
#include <math.h>

RemoteRateControl::RemoteRateControl(Listener* listener) : bitrateCalc(100)
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
	//Update bitrate calculator
	bitrateCalc.Update(time, size*8);
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
			UpdateKalman(time,time - prevTime, ts - prevTS, curSize, prevSize);
		//Move
		prevTS = ts;
		prevTime = time;
		prevSize = curSize;
		curSize = 0;
	}
	//Update size of current frame
	curSize += size;
}

void RemoteRateControl::UpdateKalman(QWORD now,QWORD tdelta, double tsdelta, DWORD framesize, DWORD prevframesize)
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
				if (overUseCount>1)
				{
					//Get target bitrate
					target =  (bitrateCalc.GetInstantAvg()+bitrateCalc.GetMinAvg()*3)/4;
					//Log	
					Log("BWE:  OverUsing bitrate:%llf max:%llf min:%llf target:%d \n",bitrateCalc.GetInstantAvg(),bitrateCalc.GetMaxAvg(),bitrateCalc.GetMinAvg(),target);
					//Overusing
					hypothesis = OverUsing;
					//Reset counter
					overUseCount=0;
					//Reset
					 bitrateCalc.Reset(now);
				} else {
					//increase counter
					overUseCount++;
					//Reset
					bitrateCalc.ResetMinMax();
				}
			}
		} else {
			//If we change state
			if (hypothesis!=UnderUsing)
			{
				//Log("BWE:  UnderUsing bitrate:%llf max:%llf min:%llf\n",bitrateCalc.GetInstantAvg(),bitrateCalc.GetMaxAvg(),bitrateCalc.GetMinAvg());
				//Reset bitrate
				bitrateCalc.Reset(now);
				//Under using, do nothing until going back to normal
				hypothesis = UnderUsing;
			}
		}
	} else {

		//If we change state
		if (hypothesis!=Normal)
		{
			//Log
			//Log("BWE:  Normal  bitrate:%llf max:%llf min:%llf\n",bitrateCalc.GetInstantAvg(),bitrateCalc.GetMaxAvg(),bitrateCalc.GetMinAvg());
			//Reset
			bitrateCalc.Reset(now);
			//Normal
			hypothesis = Normal;
		}
	}

	//If we have a new target bitrate
	if (target && listener)
		//Call listenter
		listener->onTargetBitrateRequested(target);
}

void RemoteRateControl::UpdateRTT(DWORD rtt)
{
	//Check difference
	if (this->rtt && rtt>this->rtt*1.30 && bitrateCalc.GetInstantAvg())
	{
		//Get target bitrate
		DWORD target =  bitrateCalc.GetInstantAvg();
		//Log	
		Log("BWE: RTT increase %d to %d bitrate:%llf target:%d \n",this->rtt,rtt,bitrateCalc.GetInstantAvg(),target);
		//If we have a new target bitrate
		if (target && listener)
			//Call listenter
			listener->onTargetBitrateRequested(target);
	}
	//Update RTT
	this->rtt = rtt;
}
