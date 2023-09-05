/* 
*File:   remoteratecontrol.cpp
*Author: Sergio
*
*Created on 26 de diciembre de 2012, 12:49
 */

#include "remoteratecontrol.h"
#include <cstdlib>
#include <cmath>
#include "log.h"

RemoteRateControl::RemoteRateControl() : bitrateCalc(100), fpsCalc(1000), packetCalc(100), lostCalc(100)
{
	eventSource = NULL;
	rtt = 0;
	prevTS = 0;
	prevTime = 0;
	prevSize = 0;
	prevTarget = 0;
	curTS = 0;
	curTime = 0;
	curSize = 0;
	curDelta = 0;
	prevDelta = 0;
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
	overUseCount = 0;
	absSendTimeCycles = 0;
}

void RemoteRateControl::Update(QWORD time,QWORD ts,DWORD size, bool mark)
{
	//Update bitrate calculator
	bitrateCalc.Update(time, size*8);
	//Update packet count
	packetCalc.Update(time, 1);
	
//	Debug("ts:%u,time:%llu,%u\n",ts,time,size);
	
	//If it is an out of order packet from previous frame
	if (curTS && ts < curTS)
		//Exit
		return;
	
	//Update size of current frame
	curSize += size;
	//And reception time
	curTS	= ts;
	curTime = time;
	
	//If not first packet
	if (prevTime)
		//Increase deltas
		curDelta += (curTime - prevTime) - (curTS - prevTS);
				
	//Add new frame
	if (mark) 
	{
		//New frame
		fpsCalc.Update(time,1);
		//Update Kalman filter as per google algorithm from the previous frame
		UpdateKalman(curDelta - prevDelta, curSize - prevSize);
		//reset frame stats
		prevSize = curSize;
		prevDelta = curDelta;
		curSize = 0;
		curDelta = 0;
	} 
	//Update current stats
	prevTS = curTS;
	prevTime = curTime;
}

void RemoteRateControl::UpdateKalman(int deltaTime, int deltaSize)
{
	//UltraDebug("RemoteRateControl::UpdateKalman() deltas [time:%d size:%d]\n",deltaTime, deltaSize);

	//Get scaling factor
	double scaleFactor = 30.0/fpsCalc.GetInstantAvg();

	// Update the Kalman filter
	E[0][0] += processNoise[0]*scaleFactor;
	E[1][1] += processNoise[1]*scaleFactor;

	if ((hypothesis==OverUsing && offset<prevOffset) || (hypothesis==UnderUsing && offset>prevOffset))
		E[1][1] += 10*processNoise[1]*scaleFactor;

	const double h[2] = 
	{
		(double)deltaSize,
		1.0
	};
	const double Eh[2] =
	{
		E[0][0]*h[0] + E[0][1]*h[1],
		E[1][0]*h[0] + E[1][1]*h[1]
	};

	const double residual = deltaTime-slope*h[0]-offset;

	// Only update the noise estimate if we're not over-using and in stable state
	//if (hypothesis!=OverUsing && (fmin(fpsCalc.GetAcumulated(),30)*std::fabs(offset)<threshold))
	{
		double residualFiltered = residual;

		// We try to filter out very late frames. For instance periodic key
		// frames doesn't fit the Gaussian model well.
		if (std::fabs(residual)<3*sqrt(varNoise))
			residualFiltered = 3*sqrt(varNoise);

		// Faster filter during startup to faster adapt to the jitter level
		// of the network alpha is tuned for 30 frames per second, but
		double alpha = 0.01;
		if (fpsCalc.GetAcumulated()> 60)
			alpha = 0.002;

		// beta is a function of alpha and the time delta since
		// the previous update.
		const double beta = pow(1-alpha, deltaSize*30/1000.0);
		avgNoise = beta*avgNoise + (1-beta)*residualFiltered;
		varNoise = beta*varNoise + (1-beta)*(avgNoise-residualFiltered)*(avgNoise-residualFiltered);
	}

	const double denom = varNoise+h[0]*Eh[0]+h[1]*Eh[1];
	const double K[2] = 
	{
		Eh[0] / denom,
		Eh[1] / denom
	};
	const double IKh[2][2] =
	{
		{ 1.0-K[0]*h[0] ,    -K[0]*h[1] },
		{    -K[1]*h[0] , 1.0-K[1]*h[1] }
	};

	// Update state
	E[0][0] = E[0][0]*IKh[0][0] + E[1][0]*IKh[0][1];
	E[0][1] = E[0][1]*IKh[0][0] + E[1][1]*IKh[0][1];
	E[1][0] = E[0][0]*IKh[1][0] + E[1][0]*IKh[1][1];
	E[1][1] = E[0][1]*IKh[1][0] + E[1][1]*IKh[1][1];

	slope = slope+K[0]*residual;
	prevOffset = offset;
	offset = offset+K[1]*residual;

	const double T = !fpsCalc.IsEmpty() ? fmin(fpsCalc.GetInstantAvg(),30)*offset : offset;

	//Debug("BWE: Update tdelta:%d,tsdelta:%d,fsdelta:%d,t:%f,threshold:%f,slope:%f,offset:%f,scale:%f,frames:%lld,fps:%llf,residual:%f\n",deltaTime,deltaTS,deltaSize,T,threshold,slope,offset,scaleFactor,fpsCalc.GetInstantAvg(),fpsCalc.GetInstantAvg(),residual);

	//Compare
	if (std::fabs(T)>threshold)
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
					UltraDebug("BWE: Overusing bitrate:%.0Lf max:%.0Lf min:%.0Lf T:%f,threshold:%f\n",bitrateCalc.GetInstantAvg(),bitrateCalc.GetMaxAvg(),bitrateCalc.GetMinAvg(),std::fabs(T),threshold);
					//Overusing
					hypothesis = OverUsing;
					//Reset counter
					overUseCount=0;
				} else {
					UltraDebug("BWE: Overusing bitrate:%.0Lf max:%.0Lf min:%.0Lf T:%f,threshold:%f\n",bitrateCalc.GetInstantAvg(),bitrateCalc.GetMaxAvg(),bitrateCalc.GetMinAvg(),std::fabs(T),threshold);
					//increase counter
					overUseCount++;
				}
			}
		} else {
			//If we change state
			if (hypothesis!=UnderUsing)
			{
				UltraDebug("BWE:  UnderUsing bitrate:%.0Lf max:%.0Lf min:%.0Lf T:%f\n",bitrateCalc.GetInstantAvg(),bitrateCalc.GetMaxAvg(),bitrateCalc.GetMinAvg(),std::fabs(T));
				//Reset bitrate
				bitrateCalc.ResetMinMax();
				//Under using, do nothing until going back to normal
				hypothesis = UnderUsing;
				//Reset counter
				overUseCount=0;
			}
		}
	} else {

		//If we change state
		if (hypothesis!=Normal)
		{
			//Log
			UltraDebug("BWE:  Normal  bitrate:%.0Lf max:%.0Lf min:%.0Lf\n",bitrateCalc.GetInstantAvg(),bitrateCalc.GetMaxAvg(),bitrateCalc.GetMinAvg());
			//Reset
			bitrateCalc.ResetMinMax();
			//Normal
			hypothesis = Normal;
			//Reset counter
			overUseCount=0;
		}
	}
	if (eventSource) eventSource->SendEvent("rrc.update","[%llu,\"%s\"]",getTimeMS(),GetName(hypothesis));
}

bool RemoteRateControl::UpdateRTT(DWORD rtt)
{
	//Check difference
	if (this->rtt>40 && rtt>this->rtt*1.50)
	{	
		//Overusing
		hypothesis = OverUsing;
		//Reset counter
		overUseCount=0;
	}
	
	//Update RTT
	this->rtt = rtt;

	//Debug
	UltraDebug("BWE: UpdateRTT rtt:%dms hipothesis:%s\n",rtt,GetName(hypothesis));

	if (eventSource) eventSource->SendEvent("rrc.rtt","[%llu,\"%s\",\"%d\"]",getTimeMS(),GetName(hypothesis),rtt);

	//Return if we are overusing now
	return hypothesis==OverUsing;
}

bool RemoteRateControl::UpdateLost(DWORD num)
{
	//Update lost count
	lostCalc.Update(getTime(),num);
	
	//If we are in window
	if (packetCalc.IsInWindow() && lostCalc.IsInWindow())
	{
		//Get packets
		auto packets = packetCalc.GetInstantAvg();
		auto lost    = lostCalc.GetInstantAvg();
		
		//Check lost is more than 2.5%
		if (lost*1000/(packets+lost)>25)
		{
			//Check 
			if (overUseCount>2)
			{
				//Overusing
				hypothesis = OverUsing;
				//Reset counter
				overUseCount=0;
				//Reset lost counter
				lostCalc.Reset(getTime());
				//Debug
				UltraDebug("BWE: UpdateLostlost:%d hipothesis:%s,num:%d,packets:%Lf,lost:%Lf\n",num,GetName(hypothesis),num,packets,lost);
			} else {
				//increase counter
				overUseCount++;
			}
		}
	}

	if (eventSource) eventSource->SendEvent("rrc.lost","[%llu,\"%s\",\"%d\"]",getTimeMS(),GetName(hypothesis),rtt);

	//true if overusing
	return hypothesis==OverUsing;
}

void RemoteRateControl::SetRateControlRegion(Region region)
{
	//Debug
	UltraDebug("BWE: SetRateControlRegion %s\n",GetName(region));

	switch (region)
	{
		case BelowMax:
			threshold = 35;
			break;
		case MaxUnknown:
			threshold = 25;
			break;
		case AboveMax:
		case NearMax:
			threshold = 12;
			break;
	}
}
