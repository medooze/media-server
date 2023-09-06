/* 
 * File:   remoterateestimator.cpp
 * Author: Sergio
 * 
 * Created on 8 de marzo de 2013, 10:43
 */

#include <map>
#include <cstdlib>
#include <cmath>
#include "remoterateestimator.h"

RemoteRateEstimator::RemoteRateEstimator() : bitrateAcu(200)
{
	//Not last estimate
	minConfiguredBitRate	= 128000;
	maxConfiguredBitRate	= 1280000000;
	currentBitRate		= 0;
	maxHoldRate		= 0;
	avgMaxBitRate		= -1.0f;
	varMaxBitRate		= 0.4f;
	lastBitRateChange	= 0;
	avgChangePeriod		= 1000.0f;
	lastChange		= 0;
	beta			= 0.9f;
	noiseVar 		= 0;
	rtt			= 200;
	absSendTimeCycles	= 0;
	curTS			= 0;
	//Set initial state and region
	cameFromState		= Decrease;
	state			= Hold;
	region			= RemoteRateControl::MaxUnknown;
}

RemoteRateEstimator::~RemoteRateEstimator()
{
	//Clean all streasm
	for (Streams::iterator it = streams.begin(); it!=streams.end(); ++it)
		//Delete
		delete(it->second);
}
void RemoteRateEstimator::AddStream(DWORD ssrc)
{
	Log("-RemoteRateEstimator adding stream [ssrc:%u]\n",ssrc);

	//Lock
	lock.WaitUnusedAndLock();
	//Create new control
	RemoteRateControl* ctrl = new RemoteRateControl();
	//Set tracer
	ctrl->SetEventSource(eventSource);
	//Add it
	streams[ssrc] = ctrl;
	//Unlock
	lock.Unlock();
}

void RemoteRateEstimator::RemoveStream(DWORD ssrc)
{
	Log("-RemoteRateEstimator removing stream [ssrc:%u]\n",ssrc);
	
	//Lock
	lock.WaitUnusedAndLock();
	Streams::iterator it = streams.find(ssrc);

	//If found
	if (it!=streams.end())
	{
		//Delete
		delete(it->second);
		//REmove
		streams.erase(it);
	}
	
	//Unlock
	lock.Unlock();
}

void RemoteRateEstimator::Update(DWORD ssrc,const RTPPacket::shared& packet,DWORD size)
{
	//Get rtp timestamp in ms
	QWORD ts = packet->GetClockTimestamp();

	if (packet->HasAbsSentTime())
	{
		//Use absolote time instead of rtp time for knowing timestamp at origin
		ts = packet->GetAbsSendTime()+64000*absSendTimeCycles;
		//Check if it has been a wrapp arround, absSendTime has 64s wraps
		if (ts+32000<curTS)
		{
			//Increase cycles
			absSendTimeCycles++;
			//Fix wrap for this one
			ts += 64000;
		}
	}
	
	
	//Update
	Update(packet->GetSSRC(),packet->GetTime(),ts,size, packet->GetMark());
	
	//Store current ts
	curTS = ts;
}

void RemoteRateEstimator::Update(DWORD ssrc,QWORD now,QWORD ts,DWORD size, bool mark)
{
	//Log("-Update [ssrc:%u,now:%lu,last:%u,ts:%lu,size:%u,inwindow:%d\n",ssrc,now,lastChange,ts,size,bitrateAcu.IsInWindow());
	//Lock
	lock.WaitUnusedAndLock();

	//Acumulate bitrate
	bitrateAcu.Update(now,size*8);

	//Get global usage for all streams
	RemoteRateControl::BandwidthUsage usage = RemoteRateControl::UnderUsing;

	//Check if we need to target
	bool streamOverusing = false;

	//Reset noise
	noiseVar = 0;
	
	//Check if it was an unknown stream
	if (streams.find(ssrc)==streams.end())
	{
		//Create new control
		RemoteRateControl* ctrl = new RemoteRateControl();
		//Set tracer
		ctrl->SetEventSource(eventSource);
		//Add it
		streams[ssrc] = ctrl;
	}

	//For each one
	for (Streams::iterator it = streams.begin(); it!=streams.end(); ++it)
	{
		//Get control
		RemoteRateControl *ctrl = it->second;
		//Get stream usage
		RemoteRateControl::BandwidthUsage streamUsage = ctrl->GetUsage();
		//Check if it is the new one
		if (it->first == ssrc)
		{
			//Update it
			ctrl->Update(now,ts,size, mark);
			//Check if pacekt triggered overuse
			if (streamUsage!=RemoteRateControl::OverUsing && ctrl->GetUsage()==RemoteRateControl::OverUsing)
			{
				//We are overusing now
				streamOverusing = true;
				//Set usage
				usage = RemoteRateControl::OverUsing;
			}
		} 
		//Get worst
		if (usage<streamUsage)
			//Set it
			usage = streamUsage;
		//Get noise var and sum up
		noiseVar += ctrl->GetNoise();
	}
	
	//Normalize
	noiseVar = streams.size() ? noiseVar/streams.size() : 0;

	//If not firs update
	if (!lastChange)
		//Skip the first half second
		lastChange = now+500;
	
	//Only update once per second or when the stream starts to overuse
	if (lastChange+1000<now)
	{
		//Update
		Update(usage,streamOverusing,now);
		//Reset min max
		bitrateAcu.ResetMinMax();
	} else if ( streamOverusing) {
		//Update but not reset
		Update(usage,streamOverusing,now);
	}
		
	//Unloc
	lock.Unlock();
	//Exit
	return;
}

void RemoteRateEstimator::Update(RemoteRateControl::BandwidthUsage usage, bool reactNow, QWORD now)
{
	// If it is the first estimation
	if (!currentBitRate)
		//Init to maximum
		currentBitRate = bitrateAcu.GetMaxAvg();

	//Calculate difference from last update
	QWORD changePeriod = now - lastChange;

	//Update last changed
	lastChange = now;

	//calculate average period
	avgChangePeriod = 0.9f * avgChangePeriod + 0.1f * changePeriod;

	//Modify state depending on the bandwidht state
	switch (usage)
	{
		case RemoteRateControl::Normal:
			if (state == Hold)
			{
				//Change now
				lastBitRateChange = now;
				//Swicth to increase
				ChangeState(Increase);
			} else if (state == Decrease) {
				ChangeState(Hold);
			}
			break;
		case RemoteRateControl::OverUsing:
			if (state == Increase)
				//Decrease
				ChangeState(Hold);
			else if (state == Hold)
				//Decrease
				ChangeState(Decrease);
			break;
		case RemoteRateControl::UnderUsing:
			if (region==RemoteRateControl::NearMax && state != Hold)
			{
				ChangeState(Hold);
			}
			else if (state!=Increase)
			{
				//Change now
				lastBitRateChange = now;
				//Swicth to increase
				ChangeState(Increase);
			}
			break;
	}

	//Get current estimation
	DWORD current = currentBitRate;
	//Get incoming bitrate
	float incomingBitRate = bitrateAcu.GetInstantAvg();
	// Calculate the max bit rate std dev given the normalized
	// variance and the current incoming bit rate.
	float stdMaxBitRate = sqrt(varMaxBitRate * avgMaxBitRate);

	if (stdMaxBitRate<avgMaxBitRate*0.03)
		stdMaxBitRate = avgMaxBitRate*0.03;	

	bool recovery = false;

	//Depending on curren state
	switch (state)
	{
		case Hold:
			maxHoldRate = fmax(maxHoldRate, incomingBitRate);
			break;
		case Increase:
		{
			if (avgMaxBitRate >= 0)
			{
				if (incomingBitRate > avgMaxBitRate + 3 * stdMaxBitRate)
				{
					ChangeRegion(RemoteRateControl::MaxUnknown);
					UpdateMaxBitRateEstimate(fmax(currentBitRate,incomingBitRate));
				} else if (incomingBitRate > avgMaxBitRate + 2.5 * stdMaxBitRate) {
					ChangeRegion(RemoteRateControl::AboveMax);
				} else if (incomingBitRate > avgMaxBitRate - 3 * stdMaxBitRate) {
					ChangeRegion(RemoteRateControl::NearMax);
				} else {
					ChangeRegion(RemoteRateControl::BelowMax);
				}
			}

			const DWORD responseTime = (DWORD) (avgChangePeriod + 0.5f) + rtt + 300;
			double alpha = RateIncreaseFactor(now, lastBitRateChange, responseTime);

			current = (DWORD) (current * alpha) + 8000;

			if (maxHoldRate > 0 && beta * maxHoldRate > current)
			{
				current = (DWORD) (beta * maxHoldRate);
				UpdateMaxBitRateEstimate(fmax(currentBitRate,incomingBitRate));
				ChangeRegion(RemoteRateControl::NearMax);
				recovery = true;
			}

			maxHoldRate = 0;
			UltraDebug("BWE: Increase rate to current = %u kbps\n", current / 1000);
			lastBitRateChange = now;
			break;
		}
		case Decrease:
			// Set bit rate to something slightly lower than max
			// to get rid of any self-induced delay.
			current = (DWORD) (beta * incomingBitRate + 0.5);
			if (current > currentBitRate)
			{
				// Avoid increasing the rate when over-using.
				if (region != RemoteRateControl::MaxUnknown)
					current = (DWORD) (beta * avgMaxBitRate + 0.5f);
				current = fmin(current, currentBitRate);
			}


			if (avgMaxBitRate<0 || incomingBitRate > avgMaxBitRate - 3 * stdMaxBitRate )
			{
				ChangeRegion(RemoteRateControl::NearMax);
				UpdateMaxBitRateEstimate(fmax(currentBitRate,incomingBitRate));
			} else {
				ChangeRegion(RemoteRateControl::BelowMax);
			}	

			UltraDebug("BWE: Decrease rate to current = %u kbps\n", current / 1000);
		
			lastBitRateChange = now;
			break;
	}
	
	if (!recovery && (incomingBitRate > 100000 || current > 150000) && current > 1.5 * incomingBitRate)
	{
		// Allow changing the bit rate if we are operating at very low rates
		// Don't change the bit rate if the send side is too far off
		current = currentBitRate;
		lastBitRateChange = now;
	}

	//Update
	currentBitRate = current;

	//Chec min
	if (currentBitRate<minConfiguredBitRate)
		//Set minimun
		currentBitRate = minConfiguredBitRate;
	//Chec max
	if (currentBitRate>maxConfiguredBitRate)
		//Set maximum
		currentBitRate = maxConfiguredBitRate;

	UltraDebug("BWE: estimation state=%s region=%s usage=%s currentBitRate=%d current=%d incoming=%f min=%Lf max=%Lf\n",GetName(state),RemoteRateControl::GetName(region),RemoteRateControl::GetName(usage),currentBitRate/1000,current/1000,incomingBitRate/1000,bitrateAcu.GetMinAvg()/1000,bitrateAcu.GetMaxAvg()/1000);

	if (eventSource)
		eventSource->SendEvent
		(
			"rre",
			"[%llu,\"%s\",\"%s\",%d,%d,%d,%d,%d]",
			now,
			GetName(state),
			RemoteRateControl::GetName(region),
			(DWORD)currentBitRate/1000,
			avgMaxBitRate>0?(DWORD)avgMaxBitRate/1000:0,
			(DWORD)incomingBitRate/1000,
			bitrateAcu.IsInMinMaxWindow()?(DWORD)bitrateAcu.GetMinAvg()/1000:0,
			bitrateAcu.IsInMinMaxWindow()?(DWORD)bitrateAcu.GetMaxAvg()/1000:0
		);

	//Check if we need to send inmediate feedback
	if (listener)
		//Send it
		listener->onTargetBitrateRequested(GetEstimatedBitrate(),GetEstimatedBitrate(), incomingBitRate);
}

double RemoteRateEstimator::RateIncreaseFactor(QWORD now, QWORD last, DWORD reactionTime) const
{
	// alpha = 1.02 + B ./ (1 + exp(b*(tr - (c1*s2 + c2))))
	// Parameters
	const double B = 0.0407;
	const double b = 0.0025;
	const double c1 = -6700.0 / (33 * 33);
	const double c2 = 800.0;
	const double d = 0.85;

	double alpha = 1.005 + B / (1 + exp(b * (d * reactionTime - (c1 * noiseVar + c2))));

	if (alpha < 1.005)
		alpha = 1.005;
	else if (alpha > 1.3)
		alpha = 1.3;

	if (last)
		alpha = pow(alpha, (now - last) / 1000.0);

	if (region == RemoteRateControl::NearMax)
		// We're close to our previous maximum. Try to stabilize the
		// bit rate in this region, by increasing in smaller steps.
		alpha = alpha - (alpha - 1.0) / 2.0;
	else if (region == RemoteRateControl::MaxUnknown)
		alpha = alpha + (alpha - 1.0) * 4.0;
	else if (region == RemoteRateControl::BelowMax)
		alpha = alpha + (alpha - 1.0) * 2.0;

	return alpha;
}

void RemoteRateEstimator::UpdateChangePeriod(QWORD now)
{
	QWORD changePeriod = 0;
	if (lastChange)
		changePeriod = now - lastChange;
	lastChange = now;
	avgChangePeriod = 0.9f * avgChangePeriod + 0.1f * changePeriod;
}

void RemoteRateEstimator::UpdateMaxBitRateEstimate(float incomingBitRate)
{
	const float alpha = 0.10f;
	
	if (avgMaxBitRate == -1.0f)
		avgMaxBitRate = incomingBitRate;
	else
		avgMaxBitRate = (1 - alpha) * avgMaxBitRate + alpha * incomingBitRate;

	// Estimate the max bit rate variance and normalize the variance with the average max bit rate.
	const float norm = fmax(avgMaxBitRate, 1.0f);

	varMaxBitRate = (1 - alpha) * varMaxBitRate + alpha * (avgMaxBitRate - incomingBitRate) * (avgMaxBitRate - incomingBitRate) / norm;

	// 0.4 ~= 14 kbit/s at 500 kbit/s
	if (varMaxBitRate < 0.4f)
		varMaxBitRate = 0.4f;

	// 2.5f ~= 35 kbit/s at 500 kbit/s
	if (varMaxBitRate > 2.5f)
		varMaxBitRate = 2.5f;

}

DWORD RemoteRateEstimator::GetEstimatedBitrate()
{
	//Retun estimation
	return bitrateAcu.IsInWindow() ? currentBitRate : 0;
}

void RemoteRateEstimator::GetSSRCs(std::list<DWORD> &ssrcs)
{
	//For each one
	for (Streams::iterator it = streams.begin();  it!=streams.end(); ++it)
		//add ssrc
		ssrcs.push_back(it->first);
}
void RemoteRateEstimator::ChangeState(State newState)
{
	UltraDebug("BWE: ChangeState from:%s to:%s\n",GetName(state),GetName(newState));
	//Store values
	cameFromState = state;
	state = newState;
}

void RemoteRateEstimator::ChangeRegion(RemoteRateControl::Region newRegion)
{
	UltraDebug("BWE: Change region to:%s\n",RemoteRateControl::GetName(newRegion));
	//Store new region
	region = newRegion;
	//Calculate new beta
	switch (region)
	{
		case RemoteRateControl::AboveMax:
		case RemoteRateControl::MaxUnknown:
			beta = 0.9f;
			break;
		case RemoteRateControl::NearMax:
			beta = 0.95f;
			break;
		case RemoteRateControl::BelowMax:
			beta = 0.85f;
	}
	//Set it on controls
	for (Streams::iterator it = streams.begin(); it!=streams.end(); ++it)
		//Set region on control
		 it->second->SetRateControlRegion(newRegion);
}

void RemoteRateEstimator::UpdateRTT(DWORD ssrc, DWORD rtt, QWORD now)
{
	//Lock
	lock.WaitUnusedAndLock();
	//Update
	this->rtt = rtt;
	//Find stream
	Streams::iterator it = streams.find(ssrc);
	//If found
	if (it!=streams.end())
		//Set it
		if (it->second->UpdateRTT(rtt))
			//Update
			Update(it->second->GetUsage(),true, now);
	//Unlock
	lock.Unlock();
}

void RemoteRateEstimator::UpdateLost(DWORD ssrc, DWORD lost, QWORD now)
{
	//Lock
	lock.WaitUnusedAndLock();
	//Find stream
	Streams::iterator it = streams.find(ssrc);
	//If found
	if (it!=streams.end())
		//Set it
		if (it->second->UpdateLost(lost))
			//Update
			Update(it->second->GetUsage(),true, now);
	//Unlock
	lock.Unlock();
}


void RemoteRateEstimator::SetTemporalMaxLimit(DWORD limit)
{
	Log("-RemoteRateEstimator::SetTemporalMaxLimit() %d\n",limit);
	//Check if reseting
	if (limit)
		//Set maximun bitrate
		maxConfiguredBitRate = limit;
	else
		//Set default max
		maxConfiguredBitRate = 1280000000;
}

void RemoteRateEstimator::SetTemporalMinLimit(DWORD limit)
{
	Log("-RemoteRateEstimator::SetTemporalMinLimit %d\n",limit);
	//Check if reseting
	if (limit)
		//Set maximun bitrate
		minConfiguredBitRate = limit;
	else
		//Set default min
		minConfiguredBitRate = 128000;
}
void RemoteRateEstimator::SetListener(Listener *listener)
{
	//Store listener
	this->listener = listener;
}
