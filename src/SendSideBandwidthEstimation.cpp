#include <sys/stat.h> 
#include <fcntl.h>
#include <cmath>
#include "SendSideBandwidthEstimation.h"

constexpr uint64_t kStartupDuration		= 1E6;		// 1s
constexpr uint64_t kStartupTargetBitrate	= 512000;	// 512kbps
constexpr uint64_t kMonitorDuration		= 250E3;	// 250ms
constexpr uint64_t kMonitorTimeout		= 750E3;	// 750ms
constexpr uint64_t kLongTermDuration		= 60E6;		// 60s
constexpr uint64_t kMinRate			= 128E3;	// 128kbps
constexpr uint64_t kMaxRate			= 100E6;	// 100mbps
constexpr uint64_t kMinRateChangeBps		= 16000;
constexpr double   kSamplingStep		= 0.0005f;


SendSideBandwidthEstimation::SendSideBandwidthEstimation() : 
		rttAcumulator(kLongTermDuration,1E6),
		deltaAcumulator(kMonitorDuration,1E6),
		totalSentAcumulator(kMonitorDuration,1E6),
		mediaSentAcumulator(kMonitorDuration,1E6),
		rtxSentAcumulator(kMonitorDuration,1E6),
		probingSentAcumulator(kMonitorDuration,1E6),
		totalRecvAcumulator(kMonitorDuration,1E6),
		mediaRecvAcumulator(kMonitorDuration,1E6),
		rtxRecvAcumulator(kMonitorDuration,1E6),
		probingRecvAcumulator(kMonitorDuration,1E6)

{
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation()
{
	//If  dumping
	if (fd!=FD_INVALID)
		//Close file
		close(fd);
}
	
void SendSideBandwidthEstimation::SentPacket(const PacketStats::shared& stat)
{
	//Check first packet sent time
	if (!firstSent)
		//Set first time
		firstSent = stat->time;
	
	//Add sent total
	totalSentAcumulator.Update(stat->time,stat->size);

	//Check type
	if (stat->probing)
	{
		//Update accumulators
		probingSentAcumulator.Update(stat->time,stat->size);
		rtxSentAcumulator.Update(stat->time);
		mediaSentAcumulator.Update(stat->time);
	} else if (stat->rtx) {
		//Update accumulators
		probingSentAcumulator.Update(stat->time);
		rtxSentAcumulator.Update(stat->time,stat->size);
		mediaSentAcumulator.Update(stat->time);
	} else {
		//Update accumulators
		probingSentAcumulator.Update(stat->time);
		rtxSentAcumulator.Update(stat->time);
		mediaSentAcumulator.Update(stat->time,stat->size);
	}
	
	//Add to history map
	transportWideSentPacketsStats[stat->transportWideSeqNum] = stat;
	
	//Protect against missfing feedbacks, remove too old lost packets
	auto it = transportWideSentPacketsStats.begin();
	//If there are no intervals for them
	while(it!=transportWideSentPacketsStats.end() && it->second->time+rtt+kMonitorTimeout<stat->time)
		//Erase it and move iterator
		it = transportWideSentPacketsStats.erase(it);

}

void SendSideBandwidthEstimation::ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets, uint64_t when)
{

	//Check we have packets
	if (packets.empty())
		//Skip
		return;
	
	//TODO: How to handle lost feedback packets?
	
	//Get last packets stats
	auto last = transportWideSentPacketsStats.find(packets.rbegin()->first);
	//We can use the difference between the last send packet time and the reception of the fb packet as proxy of the rtt min 
	if (last!=transportWideSentPacketsStats.end())
	{
		//Get stats
		const auto& stat = last->second;
		//Get sent time
		const auto sentTime = stat->time;

		//Double check
		if (when>sentTime)
		{
			//Calculate rtt proxy, it is guaranteed to be bigger than rtt
			uint64_t rtt = (when - sentTime)/1000;
			//Update min
			rttAcumulator.Update(when,rtt);
		}
	}
	
	//For each packet
	for (const auto& feedback : packets)
	{
		//Get feedback data
		auto transportSeqNum	= feedback.first;
		auto receivedTime	= feedback.second; 
		
		//Get packets stats
		auto it = transportWideSentPacketsStats.find(transportSeqNum);
		//If found
		if (it!=transportWideSentPacketsStats.end())
		{
			//Get stats
			const auto& stat = it->second;
			//Get sent time
			const auto sentTime = stat->time;
			
			//Check first feedback received
			if (!firstRecv)
				firstRecv = receivedTime;
			
			//Correc ts
			uint64_t fb   = when - firstSent;
			uint64_t sent = sentTime - firstSent;
			uint64_t recv = receivedTime ? receivedTime - firstRecv : 0;
			//Get deltas
			uint64_t deltaSent = sent - prevSent;
			uint64_t deltaRecv = receivedTime ? recv - prevRecv : 0;
			int64_t  delta = receivedTime ? deltaRecv - deltaSent : 0;
			
			//Accumulate delta
			accumulatedDelta += delta;
			
			//Check if we have reached bottom
			if (accumulatedDelta<0)
				//Reset
				accumulatedDelta = 0;
			//Dump stats
			//Log("recv #%u sent:%.8lu (+%.6lu) recv:%.8lu (+%.6lu) delta:%.6ld fb:%u, size:%u, bwe:%lu accumulateDelta:%lld\n",transportSeqNum,sent,deltaSent,recv,deltaRecv,delta,feedbackNum, stat->size, bandwidthEstimation, accumulatedDelta);
			
			//If dumping to file
			if (fd)
			{
				char msg[1024];
				//Create log
				int len = snprintf(msg,1024,"%.8lu|%u|%hhu|%u|%lu|%lu|%lu|%lu|%ld|%u|%u|%u|%u|%d|%d|%d\n",fb,transportSeqNum,feedbackNum, stat->size,sent,recv,deltaSent,deltaRecv,delta,GetEstimatedBitrate(),GetTargetBitrate(),GetAvailableBitrate(),rtt,stat->mark,stat->rtx,stat->probing);
				//Write it
				write(fd,msg,len);
			}
			
			//Check if it was not lost
			if (receivedTime)
			{
				//Add receive total
				totalRecvAcumulator.Update(receivedTime,stat->size);

				//Check type
				if (stat->probing)
				{
					probingRecvAcumulator.Update(receivedTime,stat->size);
					rtxRecvAcumulator.Update(receivedTime);
					mediaRecvAcumulator.Update(receivedTime);
				} else if (stat->rtx) {
					probingRecvAcumulator.Update(receivedTime);
					rtxRecvAcumulator.Update(receivedTime,stat->size);
					mediaRecvAcumulator.Update(receivedTime);
				} else {
					probingRecvAcumulator.Update(receivedTime);
					rtxRecvAcumulator.Update(receivedTime);
					mediaRecvAcumulator.Update(receivedTime,stat->size);
				}
			
				//Update last received time
				lastRecv = receivedTime;
				//And previous
				prevSent = sent;
				prevRecv = recv;
			}	

			//Erase it
			transportWideSentPacketsStats.erase(it);
		} else {
			//Log
			Warning("-SendSideBandwidthEstimation::ReceivedFeedback() | Packet not found [transportSeqNum:%u,receivedTime:%llu]\n",transportSeqNum,receivedTime);
		}
	}
	
	//Calculate new estimation
	EstimateBandwidthRate(when);
}

void SendSideBandwidthEstimation::UpdateRTT(uint64_t when, uint32_t rtt)
{
	//Store rtt
	this->rtt = rtt;
	//Smooth
	rttAcumulator.Update(when,rtt);
}

uint32_t SendSideBandwidthEstimation::GetEstimatedBitrate() const
{
	return bandwidthEstimation;
}

uint32_t SendSideBandwidthEstimation::GetAvailableBitrate() const
{
	return availableRate;
}

uint32_t SendSideBandwidthEstimation::GetMinRTT() const
{
	//If we have any value
	if (!rttAcumulator.IsEmpty())
		//Get min value over long period
		return rttAcumulator.GetMinValueInWindow();
	//Return last, must be possitive
	return rtt;
}

uint32_t SendSideBandwidthEstimation::GetTargetBitrate() const
{
	return std::min(std::max(targetBitrate,kMinRate),kMaxRate);
	
}

void SendSideBandwidthEstimation::SetState(ChangeState state)
{
	//Set number of consecutive chantes
	if (this->state == state)
		consecutiveChanges++;
	else 
		consecutiveChanges = 0;
	//Store new state
	this->state = state;
}

void SendSideBandwidthEstimation::EstimateBandwidthRate(uint64_t when)
{
	//Log("-SendSideBandwidthEstimation::EstimateBandwidthRate()\n");
	


	//Get current estimated rtt
	uint32_t rttMin		= GetMinRTT();
	int32_t rttEstimated	= rttMin + accumulatedDelta/1000;
	
	//Get bitrates
	uint64_t totalRecvBitrate	= totalRecvAcumulator.GetInstantAvg() * 8;
	uint64_t totalSentBitrate	= totalSentAcumulator.GetInstantAvg() * 8;
	uint64_t rtxSentBitrate		= rtxSentAcumulator.GetInstantAvg() * 8;
	
	//Check none of then is 0 or if delay has increased too much
	if (rttMin && rttEstimated>(50+rttMin*1.5))
	{
		//We are in congestion
		SetState(ChangeState::Congestion);
		//Set bwe as received rate
		bandwidthEstimation = totalRecvBitrate;
	} else if (mediaSentAcumulator.GetInstantAvg()*8>targetBitrate) {
		//We are overshooting
		SetState(ChangeState::OverShoot);
		//Take maximum of the spike and current value
		bandwidthEstimation = std::max<uint64_t>(bandwidthEstimation, totalRecvBitrate);
	} else {
		//We are going to increase rate again
		SetState(ChangeState::Increase);
		
		//Initial conversion factor
		double confidenceAmplifier = 1+std::log(consecutiveChanges+1);
		//Get rate change
		int64_t rateChange = std::max<uint64_t>(totalSentBitrate * confidenceAmplifier * kSamplingStep,kMinRateChangeBps);
		
		//Set it, don't allow more than a 2x rate chante
		bandwidthEstimation = std::max(bandwidthEstimation, totalSentBitrate - rtxSentBitrate + std::min<int64_t>(rateChange,totalSentBitrate));
	}
	
	//Set min/max limits
	bandwidthEstimation = std::min(std::max(bandwidthEstimation,kMinRate),kMaxRate);
	
	//Corrected rate based on loss
	//bandwidthEstimation = bandwidthEstimation * ( 1.0f - lossRate);
	
	//If rtt increasing
	if (accumulatedDelta> 0)
		//Adapt to rtt slope
		targetBitrate = bandwidthEstimation * (1.0f - static_cast<double>(accumulatedDelta) / (accumulatedDelta  + kMonitorDuration));
	else
		//Try to reach bwe
		targetBitrate = bandwidthEstimation;
	
	//Calculate rtx overhead
	double overhead = rtxSentAcumulator.GetAcumulated() && mediaSentAcumulator.GetAcumulated() ?  static_cast<double>(mediaSentAcumulator.GetAcumulated()) / (mediaSentAcumulator.GetAcumulated() + rtxSentAcumulator.GetAcumulated()) : 1.0f;

	//Available rate taking into account current rtx overhead
	availableRate = targetBitrate * overhead; 

	//Set min/max limits
	availableRate = std::min(std::max(availableRate,kMinRate),kMaxRate);


//	Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [estimate:%llubps,target:%llubps,available:%llubps,sent:%llubps,recv:%llubps,rtx=%llubps,state:%d,delta=%d,media:%u,rtx:%d,overhead:%f\n",
//		bandwidthEstimation,
//		targetBitrate,
//		availableRate,
//		totalSentBitrate,
//		totalRecvBitrate,
//		rtxSentBitrate,
//		state,
//		static_cast<int32_t>(accumulatedDelta/1000),
//		mediaSentAcumulator.GetAcumulated(),
//		rtxSentAcumulator.GetAcumulated(),
//		overhead
//	);

	//Check we have listener
	if (!listener)
		return;
	
	//Check when we have to trigger a new bwe change on the app
	if (
		//If it is the first congestion
		(state == ChangeState::Congestion && consecutiveChanges == 0) ||
		//If not overshooting and las bwe was not too long ago
		(state != ChangeState::OverShoot && lastChange + kStartupDuration < when)
	)
	{
//		Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [estimate:%llubps,target:%llubps,available:%llubps,sent:%llubps,recv:%llubps,rtx=%llubps,state:%d,delta=%d,media:%u,rtx:%d,overhead:%f,when:%llu,diff:%llu\n",
//			bandwidthEstimation,
//			targetBitrate,
//			availableRate,
//			totalSentBitrate,
//			totalRecvBitrate,
//			rtxSentBitrate,
//			state,
//			static_cast<int32_t>(accumulatedDelta/1000),
//			mediaSentAcumulator.GetAcumulated(),
//			rtxSentAcumulator.GetAcumulated(),
//			overhead,
//			when,
//			when - lastChange
//		);
		//Call listener	
		listener->onTargetBitrateRequested(availableRate);
		//Upddate last changed time
		lastChange = when;
	}
}

int SendSideBandwidthEstimation::Dump(const char* filename) 
{
	//If already dumping
	if (fd!=FD_INVALID)
		//Error
		return 0;
	
	Log("-SendSideBandwidthEstimation::Dump() [\"%s\"]\n",filename);
	
	//Open file
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600))<0)
		//Error
		return false; //Error("Could not open file [err:%d]\n",errno);

	//Done
	return 1;
}

int SendSideBandwidthEstimation::StopDump() 
{
	//If not already dumping
	if (fd==FD_INVALID)
		//Error
		return 0;
	
	Log("-SendSideBandwidthEstimation::StopDump()\n");
	
	
	//Close file
	close(fd);
	
	//No dump
	fd=FD_INVALID;

	//Done
	return 1;
}
