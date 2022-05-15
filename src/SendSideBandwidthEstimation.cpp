#include <sys/stat.h> 
#include <fcntl.h>
#include <cmath>
#include "SendSideBandwidthEstimation.h"

constexpr uint64_t kInitialDuration		= 1500E3;	// 1500ms
constexpr uint64_t kReportInterval		= 250E3;	// 250ms
constexpr uint64_t kMonitorDuration		= 250E3;	// 250ms
constexpr uint64_t kMonitorTimeout		= 750E3;	// 750ms
constexpr uint64_t kLongTermDuration		= 60E6;		// 60s
constexpr uint64_t kMinRate			= 128E3;	// 128kbps
constexpr uint64_t kMaxRate			= 100E6;	// 100mbps
constexpr uint64_t kMinRateChangeBps		= 16000;
constexpr double   kSamplingStep		= 0.05f;
constexpr double   kInitialRampUp		= 1.30f;
constexpr uint64_t kRecoveryDuration		= 75E3;




SendSideBandwidthEstimation::SendSideBandwidthEstimation() : 
		rttMin(kLongTermDuration),
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
	if (!transportWideSentPacketsStats.Set(stat->transportWideSeqNum, SendSideBandwidthEstimation::Stats{stat->time, stat->size, stat->mark, stat->rtx, stat->probing}))
		Warning("-SendSideBandwidthEstimation::SentPacket() Could not store stats for packet %u\n", stat->transportWideSeqNum);
}

void SendSideBandwidthEstimation::ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets, uint64_t when)
{
	//Extend seq num
	feedbackNumExtender.Extend(feedbackNum);

	//Get extended one
	uint64_t extFeedbabkNum = feedbackNumExtender.GetExtSeqNum();
	//Check if there was a missing packet
	if (lastFeedbackNum && extFeedbabkNum > lastFeedbackNum + 1)
	{
		//Log
		UltraDebug("-SendSideBandwidthEstimation::ReceivedFeedback() missing feedback [seqNum:%u,extSeqNum:%llu,last:%llu]\n", feedbackNum, extFeedbabkNum, lastFeedbackNum);
		//Reset received accumulator
		totalRecvAcumulator.Reset(0);
	}
	
	//Store last received feedback
	lastFeedbackNum = extFeedbabkNum;

	//Check we have packets
	if (packets.empty())
		//Skip
		return;
	
	//Get last packets stats
	auto last = transportWideSentPacketsStats.Get(packets.rbegin()->first);
	//We can use the difference between the last send packet time and the reception of the fb packet as proxy of the rtt min 
	if (last.has_value())
	{
		//Get sent time
		const auto sentTime = last->time;

		//Double check
		if (when>sentTime)
		{
			//Calculate rtt proxy, it is guaranteed to be bigger than rtt
			uint64_t rtt = (when - sentTime)/1000;
			//Update min
			rttMin.Add(rtt, when);
		}
	}
	
	//For each packet
	for (const auto& feedback : packets)
	{
		//Get feedback data
		auto transportSeqNum	= feedback.first;
		auto receivedTime	= feedback.second; 

		//Get packet
		auto stat = transportWideSentPacketsStats.Get(transportSeqNum);
		
		//If found
		if (stat.has_value())
		{
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
			if (fd!=FD_INVALID)
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
		} else {
			//Log
			Warning("-SendSideBandwidthEstimation::ReceivedFeedback() | Packet not found [transportSeqNum:%u,receivedTime:%llu,first:%llu,last:%llu]\n", transportSeqNum, receivedTime, transportWideSentPacketsStats.GetFirstSeq(), transportWideSentPacketsStats.GetLastSeq());
		}
	}
	
	//Calculate new estimation
	EstimateBandwidthRate(when);
}

void SendSideBandwidthEstimation::UpdateRTT(uint64_t when, uint32_t rtt)
{
	//Store rtt
	this->rtt = rtt;
	//Calculate minimum
	rttMin.Add(when, rtt);
}

uint32_t SendSideBandwidthEstimation::GetEstimatedBitrate() const
{
	return bandwidthEstimation;
}

uint32_t SendSideBandwidthEstimation::GetAvailableBitrate() const
{
	return availableRate;
}

uint32_t SendSideBandwidthEstimation::GetTotalInstantSentBitrate() const
{
	return totalSentAcumulator.GetInstantAvg() * 8;
}

uint32_t SendSideBandwidthEstimation::GetMinRTT() const
{
	//Get minimum
	auto min = rttMin.GetMin();
	//If no data 
	if (!min)
		//return latest rtt
		return rtt;
	//Done
	return *min;
}

uint32_t SendSideBandwidthEstimation::GetTargetBitrate() const
{
	return std::min(std::max(targetBitrate,kMinRate),kMaxRate);
	
}

void SendSideBandwidthEstimation::SetState(ChangeState state)
{
	//Log("-SendSideBandwidthEstimation::SetState() [state:%d,prev:%d,consecutiveChanges:%d]\n",state,this->state,consecutiveChanges);
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
	//Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [lastChane:%lld,when:%lldd]\n",lastChange,when);
	
	//Get current estimated rtt
	uint32_t rttMin		= GetMinRTT();
	int32_t rttEstimated	= rttMin + accumulatedDelta/1000;
	
	//Get bitrates
	uint64_t totalRecvBitrate	= totalRecvAcumulator.GetInstantAvg() * 8;
	uint64_t totalSentBitrate	= totalSentAcumulator.GetInstantAvg() * 8;
	uint64_t rtxSentBitrate		= rtxSentAcumulator.GetInstantAvg() * 8;
	
	//Check none of then is 0 or if delay has increased too much
	if (state==ChangeState::Initial) 
	{
		//If first state 
		if (!lastChange)
			//Set to now
			lastChange = when;
		//Wait until we have  enought data
		else if (lastChange + kInitialDuration < when)
			//We are going to increase rate again
			SetState(ChangeState::Increase);
		//If the other side has received enought data
		if (!totalRecvAcumulator.IsInWindow())
			//Ignore first reports
			return;
		//Set bwe as received rate
		bandwidthEstimation = totalRecvBitrate;
	} else if (rttMin && rttEstimated>(20+rttMin*1.3)) {
		//We are in congestion
		SetState(ChangeState::Congestion);
		//If there was any feedback loss
		if (!totalRecvAcumulator.IsInWindow())
			//Wait until we have more feedback
			return;
		//Set bwe as received rate
		bandwidthEstimation = totalRecvBitrate;
	} else if (mediaSentAcumulator.GetInstantAvg()*8>targetBitrate) {
		//We are overshooting
		SetState(ChangeState::OverShoot);
		//Take maximum of the spike and current value
		bandwidthEstimation = std::max<uint64_t>(bandwidthEstimation, totalRecvBitrate);
	} else if (state==ChangeState::Congestion || state == ChangeState::OverShoot) {
		//We are going to conservatively reach the previous estimation again
		SetState(ChangeState::Recovery);
	} else if (state == ChangeState::Recovery) {
		//If there was enough data
		if (totalRecvAcumulator.IsInWindow())
			//Decrease bwe to converge to the received rate
			bandwidthEstimation = bandwidthEstimation * 0.99 + totalRecvBitrate * 0.01;

		//Initial conversion factor
		double confidenceAmplifier = 1 + std::log(consecutiveChanges + 1);
		//Get rate change
		int64_t rateChange = std::max<uint64_t>(totalSentBitrate * confidenceAmplifier * kSamplingStep, kMinRateChangeBps);

		//Increase the target rate
		targetBitrate = std::min(bandwidthEstimation, targetBitrate + rateChange);

		//When we have reached the bwe
		if (targetBitrate == bandwidthEstimation)
			//We are going to increase rate again
			SetState(ChangeState::Increase);
	} else if (state==ChangeState::Increase) {
		//Initial conversion factor
		double confidenceAmplifier = 1+std::log(consecutiveChanges+1);
		//Get rate change
		int64_t rateChange = std::max<uint64_t>(totalSentBitrate * confidenceAmplifier * kSamplingStep, kMinRateChangeBps);
		
		//Set it, don't allow more than a 2x rate chante
		bandwidthEstimation = std::max(bandwidthEstimation, totalSentBitrate - rtxSentBitrate + std::min<int64_t>(rateChange,totalSentBitrate));
	}
	
	//Set min/max limits
	bandwidthEstimation = std::min(std::max(bandwidthEstimation,kMinRate),kMaxRate);
	
	//If rtt increasing
	if (accumulatedDelta>2000)
		//Adapt to rtt slope
		targetBitrate = bandwidthEstimation * (1 - static_cast<double>(accumulatedDelta) / (accumulatedDelta  + kRecoveryDuration));
	else if (state == ChangeState::Initial)
		//Increase to send probing
		targetBitrate = bandwidthEstimation * kInitialRampUp;
	else if (state != ChangeState::Recovery)
		//Try to reach bwe
		targetBitrate = bandwidthEstimation;

	//Get sent data
	auto rtxSent	= rtxSentAcumulator.GetAcumulated();
	auto mediaSent	= mediaSentAcumulator.GetAcumulated();
	auto totalSent	= rtxSent + mediaSent;
	
	//Calculate long term rtx overhead
	double overhead = totalSent ?  static_cast<double>(mediaSent)/totalSent : 1.0f;

	//Available rate taking into account current rtx overhead
	availableRate = targetBitrate * overhead; 

	//Set min/max limits
	availableRate = std::min(std::max(availableRate,kMinRate),kMaxRate);


	//Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [this:%p,estimate:%llubps,target:%llubps,available:%llubps,sent:%llubps,recv:%llubps,rtx=%llubps,state:%d,delta=%d,media:%u,rtx:%d,overhead:%.2f,rttEstimated:%d,rttMin:%d\n",
	//	this,
	//	bandwidthEstimation,
	//	targetBitrate,
	//	availableRate,
	//	totalSentBitrate,
	//	totalRecvBitrate,
	//	rtxSentBitrate,
	//	state,
	//	static_cast<int32_t>(accumulatedDelta/1000),
	//	mediaSentAcumulator.GetAcumulated(),
	//	rtxSentAcumulator.GetAcumulated(),
	//	(1-overhead),
	//	rttEstimated,
	//	rttMin
	//);

	//Check we have listener
	if (!listener)
		return;
	
	//Check when we have to trigger a new bwe change on the app
	if (state != ChangeState::Initial && ((state == ChangeState::Congestion && consecutiveChanges==0) || ( lastChange + kReportInterval < when)))
	{
		//Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [estimate:%llubps,target:%llubps,available:%llubps,sent:%llubps,recv:%llubps,rtx=%llubps,state:%d,delta=%d,media:%u,rtx:%d,overhead:%f,when:%llu,diff:%llu\n",
		//	bandwidthEstimation,
		//	targetBitrate,
		//	availableRate,
		//	totalSentBitrate,
		//	totalRecvBitrate,
		//	rtxSentBitrate,
		//	state,
		//	static_cast<int32_t>(accumulatedDelta/1000),
		//	mediaSentAcumulator.GetAcumulated(),
		//	rtxSentAcumulator.GetAcumulated(),
		//	(1-overhead),
		//	when,
		//	when - lastChange
		//);
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
