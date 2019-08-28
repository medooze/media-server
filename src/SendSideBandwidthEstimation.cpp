#include <sys/stat.h> 
#include <fcntl.h>
#include <cmath>
#include "SendSideBandwidthEstimation.h"

constexpr uint64_t kStartupDuration		= 1E6;		// 1s
constexpr uint64_t kStartupTargetBitrate	= 512000;	// 512kbps
constexpr uint64_t kMonitorDuration		= 500E3;	// 500ms
constexpr uint64_t kMonitorTimeout		= 750E3;	// 750ms
constexpr uint64_t kMinRate			= 128E3;	// 128kbps
constexpr uint64_t kMaxRate			= 100E6;	// 100mbps
constexpr uint64_t kMinRateChangeBps		= 4000;
constexpr uint64_t kConversionFactor		= 100;
constexpr double   kSamplingStep		= 0.05;

// Utility function parameters.
constexpr double kDelayGradientCoefficient = 900;
constexpr double kLossCoefficient = 11.35;
constexpr double kThroughputPower = 0.9;


bool MonitorInterval::SentPacket(uint64_t sent, uint64_t size, bool probing, bool rtx)
{
	//Check it is from this interval
	if (sent<start || sent>GetEndTime())
		//Skip
		return false; //Error("-MonitorInterval::SentPacket() Skip %llu [start:%llu,end:%llu]\n",sent,start,GetEndTime());
	//Check first
	if (firstSent == std::numeric_limits<uint64_t>::max())
		firstSent = sent;
	//Set last
	lastSent = sent;
	//Append size
	accumulatedSentSize += size;
	//One more
	totalSentPackets++;
	
	//Check type
	if (probing)
		accumulatedSentProbingSize += size;
	else if (rtx)
		accumulatedSentRTXSize += size;
	else 
		accumulatedSentMediaSize += size;
	
	//Log("-MonitorInterval::SentPacket() adding %llu size=%llu [start:%llu,end:%llu,acu=%llu]\n",sent,size,start,GetEndTime(),accumulatedSentSize);
	//Accepted
	return true;
}

bool MonitorInterval::Feedback(uint64_t sent, uint64_t recv, uint64_t size, int64_t delta, bool probing, bool rtx)
{
	//Check it is from this interval 
	if (sent<start)
		//Skip packet
		return false; //Error("-MonitorInterval::Feedback() Skip %llu [start:%llu,end:%llu]\n",sent,start,GetEndTime());
	// Here we assume that if some packets are reordered with packets sent
	// after the end of the monitor interval, then they are lost. (Otherwise
	// it is not clear how long should we wait for packets feedback to arrive).
	if (sent > GetEndTime())
	{
		//Completed
		feedbackCollectionDone = true;
		//Skip
		return false; //Error("-MonitorInterval::Feedback() done, skipping %llu [start:%llu,end:%llu]\n",sent,start,GetEndTime());
	}

	//One more
	totalFeedbackedPackets++;
	
	//Check recv time
	if (recv!=std::numeric_limits<uint64_t>::max())
	{
		//Check first
		if (firstRecv == std::numeric_limits<uint64_t>::max())
			//Its first
			firstRecv = recv;
		//Set last
		lastRecv = recv;
		//Increase accumulated received sie
		accumulatedReceivedSize += size;
		//Check type
		if (probing)
			accumulatedReceivedMediaSize += size;
		else if (rtx)
			accumulatedReceivedRTXSize += size;
		else 
			accumulatedReceivedMediaSize += size;
		//Store deltas
		acumulatedDelta += delta;
		deltas.emplace_back(sent,delta);
	} else {
		//Increase number of packets losts
		lostPackets ++;
	}
	
	//Log("-MonitorInterval::Feedback() %llu size=%llu [start:%llu,end:%llu,acu=%llu]\n",sent,size,start,GetEndTime(),accumulatedReceivedSize);
	
	//Accepted
	return true;
}

uint64_t MonitorInterval::GetSentBitrate() const
{
	return lastSent!=firstSent ? (accumulatedSentSize * 8E6) / (lastSent - firstSent) : 0; 
}

uint64_t MonitorInterval::GetSentEffectiveBitrate() const
{
	return lastSent!=firstSent ? ((accumulatedSentSize - accumulatedSentRTXSize)  * 8E6) / (lastSent - firstSent) : 0; 
}

uint64_t MonitorInterval::GetSentMediaBitrate() const
{
	return lastSent!=firstSent ? (accumulatedSentMediaSize * 8E6) / (lastSent - firstSent) : 0; 
}

uint64_t MonitorInterval::GetSentProbingBitrate() const
{
	return lastSent!=firstSent ? (accumulatedSentProbingSize * 8E6) / (lastSent - firstSent) : 0; 
}

uint64_t MonitorInterval::GetSentRTXBitrate() const
{
	return lastSent!=firstSent ? (accumulatedSentRTXSize * 8E6) / (lastSent - firstSent) : 0; 
}

uint64_t MonitorInterval::GetReceivedBitrate() const
{
	return lastRecv!=firstRecv ? (accumulatedReceivedSize * 8E6) / (lastRecv - firstRecv) : 0; 
}

uint64_t MonitorInterval::GetReceivedMediaBitrate() const
{
	return lastRecv!=firstRecv ? (accumulatedReceivedMediaSize * 8E6) / (lastRecv - firstRecv) : 0; 
}

uint64_t MonitorInterval::GetReceivedProbingBitrate() const
{
	return lastRecv!=firstRecv ? (accumulatedReceivedProbingSize * 8E6) / (lastRecv - firstRecv) : 0; 
}

uint64_t MonitorInterval::GetReceivedRTXBitrate() const
{
	return lastRecv!=firstRecv ? (accumulatedReceivedRTXSize * 8E6) / (lastRecv - firstRecv) : 0; 
}

double MonitorInterval::GetLossRate() const
{
	return totalFeedbackedPackets ? static_cast<double> (lostPackets) / totalFeedbackedPackets : 0;
}


// For the formula used in computations see formula for "slope" in the second method:
// https://www.johndcook.com/blog/2008/10/20/comparing-two-ways-to-fit-a-line-to-data/
double MonitorInterval::ComputeDelayGradient() const
{
	double timeSum = 0;
	double delaySum = 0;
	for (const auto& delta : deltas)
	{
		double timeDelta = delta.first;
		double delay = delta.second;
		timeSum += timeDelta;
		delaySum += delay;
	}
	double squaredScaledTimeDeltaSum = 0;
	double scaledTimeDeltaDelay = 0;
	for (const auto& delta : deltas)
	{
		double timeDelta = delta.first;
		double delay = delta.second;
		double scaledTimeDelta = timeDelta - timeSum / deltas.size();
		squaredScaledTimeDeltaSum += scaledTimeDelta * scaledTimeDelta;
		scaledTimeDeltaDelay += scaledTimeDelta * delay;
	}
	return squaredScaledTimeDeltaSum ? scaledTimeDeltaDelay / squaredScaledTimeDeltaSum : 0;
}

double MonitorInterval::ComputeVivaceUtilityFunction() const 
{
	// Get functio inputs
	double bitrate		= GetSentEffectiveBitrate();
	double lossrate		= GetLossRate();
	double delayGradient	= ComputeDelayGradient();
	//Calculate the utility
	return std::pow(bitrate, kThroughputPower) - (kDelayGradientCoefficient * delayGradient * bitrate) - (kLossCoefficient * lossrate * bitrate);
}

void MonitorInterval::Dump() const
{
	Log("[MonitorInterval from=%llu to=%llu duration=%llu target=%lubps sent=%llubps recv=%llubps firstSent=%llu lastSent=%llu firstRecv=%llu lastRecv=%llu sentSize=%llu recvSize=%llu totalSent=%lu totalFeedbacked=%lu lost=%lu done=%d/]\n",
		start,
		GetEndTime(),
		duration,
		target,
		GetSentBitrate(),
		GetReceivedBitrate(),
		firstSent,
		lastSent,
		firstRecv,
		lastRecv,
		accumulatedSentSize,
		accumulatedReceivedSize,
		totalSentPackets,
		totalFeedbackedPackets,
		lostPackets,
		feedbackCollectionDone);
}

SendSideBandwidthEstimation::SendSideBandwidthEstimation() : 
		rttAccumulator(20000)
{
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation()
{
	//If  dumping
	if (fd!=FD_INVALID)
		//Close file
		close(fd);
}
	
void SendSideBandwidthEstimation::SentPacket(const PacketStats::shared& stats)
{
	
	//Check first packet sent time
	if (!firstSent)
	{
		//Set first time
		firstSent = stats->time;
		//Create initial interval
		monitorIntervals.emplace_back(kStartupTargetBitrate,0,kStartupDuration);
	}
	
	//Get sent time
	uint64_t sentTime = stats->time-firstSent;
	
	//Log("sent #%u sent:%.8lu\n",stats->transportWideSeqNum,sentTime);
	
	//For all intervals
	for (auto& interval : monitorIntervals)
		//Pas it
		interval.SentPacket(sentTime, stats->size, stats->probing, stats->rtx);
	
	//Check if last interval has already expired 
	if (sentTime > (monitorIntervals.back().GetEndTime() + rtt + kMonitorTimeout))
	{
		//Calculate new estimation
		EstimateBandwidthRate();
		//ReCreate new intervals again
		CreateIntervals(sentTime);
	}
	
	//Add to history map
	transportWideSentPacketsStats[stats->transportWideSeqNum] = stats;
	//Protect against missfing feedbacks, remove too old lost packets
	auto it = transportWideSentPacketsStats.begin();
	//If there are no intervals for them
	while(it!=transportWideSentPacketsStats.end() && (stats->time-firstSent)<monitorIntervals.front().GetStartTime())
		//Erase it and move iterator
		it = transportWideSentPacketsStats.erase(it);

}

void SendSideBandwidthEstimation::ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets, uint64_t when)
{

	//Check we have packets
	if (packets.empty())
		//Skip
		return;
	
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
			rttAccumulator.Update(when/100,rtt);
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
			accumulateDelta += delta;
			
			//Check if we have reached bottom
			if (accumulateDelta<0)
				//Reset
				accumulateDelta = GetMinRTT()/1000;
			//Dump stats
			//Log("recv #%u sent:%.8lu (+%.6lu) recv:%.8lu (+%.6lu) delta:%.6ld fb:%u, size:%u, bwe:%lu accumulateDelta:%lld\n",transportSeqNum,sent,deltaSent,recv,deltaRecv,delta,feedbackNum, stat->size, bandwidthEstimation, accumulateDelta);
			
			bool completed = true;
			//For all intervals
			for (auto& interval : monitorIntervals)
			{
				//Pas it
				interval.Feedback(sent,receivedTime? recv : std::numeric_limits<uint64_t>::max(),stat->size,delta,stat->probing,stat->rtx);
				//Check if they are completed
				completed &= interval.IsFeedbackCollectionDone();
			}
			
			//If all intervals are completed now
			if (completed)
			{
				//Calculate new estimation
				EstimateBandwidthRate();
				//Create new intervals
				CreateIntervals(sent);
			}

			//If dumping to file
			if (fd)
			{
				char msg[1024];
				//Create log
				int len = snprintf(msg,1024,"%.8lu|%u|%u|%u|%lu|%lu|%lu|%lu|%ld|%u|%u|%u|%u|%d|%d|%d\n",fb,transportSeqNum,feedbackNum, stat->size,sent,recv,deltaSent,deltaRecv,delta,GetEstimatedBitrate(),GetTargetBitrate(),GetAvailableBitrate(),rtt,stat->mark,stat->rtx,stat->probing);
				//Write it
				write(fd,msg,len);
			}

			//Check if it was not lost
			if (receivedTime)
			{
				//Update last received time
				lastRecv = receivedTime;
				//And previous
				prevSent = sent;
				prevRecv = recv;
			}	

			//Erase it
			transportWideSentPacketsStats.erase(it);
		}
	}
}

void SendSideBandwidthEstimation::UpdateRTT(uint64_t when, uint32_t rtt)
{
	//Store rtt
	this->rtt = rtt;
	//Smooth
	rttAccumulator.Update(when/1000,rtt);
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
	if (rttAccumulator.GetCount())
		//Get min value over long period
		return rttAccumulator.GetMinValueInWindow();
	//Return last, must be possitive
	return rtt;
}

uint32_t SendSideBandwidthEstimation::GetTargetBitrate() const
{
	//Find current interval
	for (const auto& interval : monitorIntervals)
	{
		//If is not completed yet
		if (!interval.IsFeedbackCollectionDone())
		{
			//This rates
			uint64_t target		= interval.GetTargetBitrate();
			uint64_t received	= interval.GetReceivedBitrate();
			
			//Check if we are overshooting
			if (!received || received > target)
				//Target is still ok
				return target;
			//Get current estimated rtt
			uint32_t rttMin		= GetMinRTT();
			int32_t rttEstimated	= accumulateDelta/1000;
			//Check none of then is 0 or if delay has decreased
			if (!rttMin || !rttEstimated || rttEstimated<rttMin)
				//Target is still ok
				return target;
			//Calculate rtt increase inverse factor
			double factor = static_cast<double>(rttMin) / rttEstimated;
			//Calculate corrected target bitrate to drain networkqueue
			uint64_t corrected = received * factor;
			//Log("-target %llu received %llu corrected:%llu rttMin:%u rttEstimaded:%d factor:%f\n",target,received,corrected,rttMin,rttEstimated,factor);
			return corrected;
		
		}
	}
	return bandwidthEstimation ? bandwidthEstimation : kStartupTargetBitrate;
}


void SendSideBandwidthEstimation::CreateIntervals(uint64_t time)
{
	//Log("-SendSideBandwidthEstimation::CreateIntervals() [time:%llu]\n",time);
	
	//Drop previous ones
	monitorIntervals.clear();
	
	//Ramdomize if we have to increase or decrease rate first
	int64_t sign = 2 * (std::rand() % 2) - 1;
	
	//Calculate step
	uint64_t step = std::max(static_cast<uint64_t>(bandwidthEstimation * kSamplingStep),kMinRateChangeBps);

	//Get current estimated rtt
	uint32_t rttMin		= GetMinRTT();
	int32_t rttEstimated	= accumulateDelta/1000;
	
	//Check none of then is 0 or if delay has increased too much
	if (rttMin  && rttEstimated && rttEstimated>(10+rttMin*1.5))
	{
		//Calculate rtt increase inverse factor
		double factor = static_cast<double>(rttMin) / rttEstimated;
		//Calculate probing btirates for monitors
		uint64_t monitorIntervalsBitrates[2] = {
			bandwidthEstimation  - step,
			bandwidthEstimation  + step * factor
		};
		//Create two consecutive monitoring intervals
		monitorIntervals.emplace_back(monitorIntervalsBitrates[0], time, kMonitorDuration);
		monitorIntervals.emplace_back(monitorIntervalsBitrates[1], time + kMonitorDuration, kMonitorDuration/2);
	} else {
		//Calculate probing btirates for monitors
		uint64_t monitorIntervalsBitrates[2] = {
			bandwidthEstimation  + sign * step,
			bandwidthEstimation  - sign * step
		};
		
		//Create two consecutive monitoring intervals
		monitorIntervals.emplace_back(monitorIntervalsBitrates[0], time, kMonitorDuration);
		monitorIntervals.emplace_back(monitorIntervalsBitrates[1], time + kMonitorDuration, kMonitorDuration);
	}
	
	//For all packets with still no feedback
	for (const auto& entry : transportWideSentPacketsStats)
	{
		//Get sent time
		uint64_t sentTime = entry.second->time - firstSent;
		uint64_t size = entry.second->size;
		//For all new intervals
		for (auto& interval : monitorIntervals)
			//Pas it
			interval.SentPacket(sentTime, size, entry.second->probing, entry.second->rtx);
	}
	
}

void SendSideBandwidthEstimation::EstimateBandwidthRate()
{
	//Log("-SendSideBandwidthEstimation::EstimateBandwidthRate()\n");
	
	if (monitorIntervals.empty())
		return;
//	
//	for (const auto& interval : monitorIntervals)
//		interval.Dump();
	
	//If startup phase completed
	if (monitorIntervals.size()==1)
	{
		//Calculate initial value based on received data so far
		bandwidthEstimation = monitorIntervals[0].GetReceivedBitrate();
		//Get sizes
		uint64_t mediaSize = monitorIntervals[0].GetSentMediaBitrate();
		uint64_t rtxSize   = monitorIntervals[0].GetSentRTXBitrate();
		//Available rate
		availableRate = rtxSize ? bandwidthEstimation * mediaSize / (mediaSize + rtxSize) : bandwidthEstimation;
		//Set new extimate
		if (listener)
			listener->onTargetBitrateRequested(availableRate);
		//Done
		return;
	}

	//Get deltas gradients
//	double delta0	= monitorIntervals[0].ComputeDelayGradient();
//	double delta1	= monitorIntervals[1].ComputeDelayGradient();
	//Calcualte utilities for each interval
	double utility0 = monitorIntervals[0].ComputeVivaceUtilityFunction();
	double utility1 = monitorIntervals[1].ComputeVivaceUtilityFunction();
	//Get actual sent rate
	double bitrate0 = std::max(monitorIntervals[0].GetSentEffectiveBitrate(),monitorIntervals[0].GetTargetBitrate());
	double bitrate1 = std::max(monitorIntervals[1].GetSentEffectiveBitrate(),monitorIntervals[1].GetTargetBitrate());
	//Get sizes
	uint64_t mediaSize = monitorIntervals[0].GetSentMediaBitrate() + monitorIntervals[1].GetSentMediaBitrate();
	uint64_t rtxSize   = monitorIntervals[0].GetSentRTXBitrate() + monitorIntervals[1].GetSentRTXBitrate();
	//Get actual target bitrate
	uint64_t targetBitrate = bitrate0 && bitrate1 ? (bitrate0 + bitrate1) / 2 : bitrate0 + bitrate1;
	
	//The utility gradient
	double gradient = (utility0 - utility1) / (bitrate0 - bitrate1);

	//Get previous state change
	auto prevState = state;

	//Check if we have sent much more than expected
	if (targetBitrate>std::max(monitorIntervals[0].GetTargetBitrate(),monitorIntervals[1].GetTargetBitrate()))
	{
		//We are overshooting
		state = ChangeState::OverShoot;
		//Update target bitrate to max received
		bandwidthEstimation = std::min(
			std::max(monitorIntervals[0].GetSentEffectiveBitrate(),monitorIntervals[1].GetSentEffectiveBitrate()),
			std::max(monitorIntervals[0].GetReceivedBitrate(),monitorIntervals[1].GetReceivedBitrate())
		);
	} else {
		//Get state from gradient
		state = gradient ? ChangeState::Increase : ChangeState::Decrease;

		//Set bumber of consecutive chantes
		if (prevState == state && state!=ChangeState::OverShoot)
			consecutiveChanges++;
		else
			consecutiveChanges = 0;

		//Initial conversion factor
		double confidenceAmplifier = 1+std::log(consecutiveChanges+1);
		//Get rate change
		int64_t rateChange = gradient * confidenceAmplifier * kConversionFactor;
		
		//Set it, don't allow more than a 2x rate chante
		bandwidthEstimation = targetBitrate + std::min<int64_t>(rateChange,targetBitrate);

		//Adjust max/min rates
		bandwidthEstimation = std::min(std::max(bandwidthEstimation,kMinRate),kMaxRate);
	
		//Get loss rate
		double lossRate = std::max(monitorIntervals[0].GetLossRate(),monitorIntervals[1].GetLossRate());
		
		//Corrected rate
		bandwidthEstimation = bandwidthEstimation * ( 1 - lossRate);
	}
	
	//Available rate
	availableRate = rtxSize ? bandwidthEstimation * mediaSize / (mediaSize + rtxSize) : bandwidthEstimation;
	
//	Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [estimate:%llubps,target:%llubps,available:%llubps,bitrate0:%llubps,bitrate1:%llubps,state:%d\n",
//		bandwidthEstimation,
//		targetBitrate,
//		availableRate,
//		static_cast<uint64_t>(bitrate0),
//		static_cast<uint64_t>(bitrate1),
//		state
//	);
	
	bandwidthEstimation = availableRate;
	
	//Set new extimate
	if (listener)
		//Call listener	
		listener->onTargetBitrateRequested(availableRate);
}

int SendSideBandwidthEstimation::Dump(const char* filename) 
{
	//If already dumping
	if (fd!=FD_INVALID)
		//Error
		return 0;
	
	Log("-SendSideBandwidthEstimation::Dump [\"%s\"]\n",filename);
	
	//Open file
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600))<0)
		//Error
		return false; //Error("Could not open file [err:%d]\n",errno);

	//Done
	return 1;
}
