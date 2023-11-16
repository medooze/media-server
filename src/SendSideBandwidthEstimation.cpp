#include <sys/stat.h> 
#include <fcntl.h>
#include <cmath>
#include "SendSideBandwidthEstimation.h"

constexpr uint64_t kInitialDuration		= 500E3;	// 500ms
constexpr uint64_t kReportInterval		= 250E3;	// 250ms
constexpr uint64_t kMonitorDuration		= 150E3;	// 150ms
constexpr uint64_t kLongTermDuration		= 10E6;		// 10s
constexpr uint64_t kMinRate			= 128E3;	// 128kbps
constexpr uint64_t kMaxRate			= 100E6;	// 100mbps
constexpr uint64_t kMinRateChangeBps		= 10000;
constexpr double   kSamplingStep		= 0.05f;
constexpr double   kInitialRampUp		= 1.30f;
constexpr uint64_t kRecoveryDuration		= 250E3;
constexpr double   LoosRateThreshold		= 0.35;		// 35% packet loss before moving to loosy state


SendSideBandwidthEstimation::SendSideBandwidthEstimation() : 
		rttMin(kLongTermDuration),
		accumulatedDeltaMinCounter(kLongTermDuration),
		totalSentAcumulator(kMonitorDuration, 1E6, 500),
		mediaSentAcumulator(kMonitorDuration, 1E6, 500),
		rtxSentAcumulator(kMonitorDuration, 1E6, 100),
		probingSentAcumulator(kMonitorDuration, 1E6, 100),
		totalRecvAcumulator(kMonitorDuration, 1E6, 500),
		mediaRecvAcumulator(kMonitorDuration, 1E6, 500),
		rtxRecvAcumulator(kMonitorDuration, 1E6, 100),
		probingRecvAcumulator(kMonitorDuration,	1E6, 100),
		packetsReceivedAcumulator(kMonitorDuration, 1E6, 500),
		packetsLostAcumulator(kMonitorDuration, 1E6, 100)
{
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation()
{
	//If  dumping
	if (fd!=FD_INVALID)
		//Close file
		close(fd);
}
	
void SendSideBandwidthEstimation::SentPacket(const PacketStats& stat)
{
	//Check first packet sent time
	if (!firstSent)
		//Set first time
		firstSent = stat.time;
	
	//Add sent total
	totalSentAcumulator.Update(stat.time,stat.size);

	//Check type
	if (stat.probing)
	{
		//Update accumulators
		probingSentAcumulator.Update(stat.time,stat.size);
		rtxSentAcumulator.Update(stat.time);
		mediaSentAcumulator.Update(stat.time);
	} else if (stat.rtx) {
		//Update accumulators
		probingSentAcumulator.Update(stat.time);
		rtxSentAcumulator.Update(stat.time,stat.size);
		mediaSentAcumulator.Update(stat.time);
	} else {
		//Update accumulators
		probingSentAcumulator.Update(stat.time);
		rtxSentAcumulator.Update(stat.time);
		mediaSentAcumulator.Update(stat.time,stat.size);
	}
	
	//Add to history map
	if (!transportWideSentPacketsStats.Set(stat.transportWideSeqNum, SendSideBandwidthEstimation::Stats{stat.time, stat.size, stat.mark, stat.rtx, stat.probing}))
		Warning("-SendSideBandwidthEstimation::SentPacket() Could not store stats for packet %u\n", stat.transportWideSeqNum);
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

	//Reset delta accumulated during this period
	lastFeedbackDelta = 0;

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
			rttMin.Add(when, rtt);
		}
	}

	//For each packet
	for (const auto& feedback : packets)
	{
		//We need to wrap the sequence number as the rtcp reports calculates it as base+counter
		// which is required to be able to retrieve the packets in increasing order her
		uint16_t transportSeqNum	= static_cast<uint16_t>(feedback.first);
		uint64_t receivedTime		= feedback.second; 

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
			int64_t deltaSent = sent - prevSent;
			int64_t deltaRecv = receivedTime ? recv - prevRecv : 0;
			int64_t delta     = receivedTime ? deltaRecv - deltaSent : 0;
			
			//Accumulate delta
			lastFeedbackDelta += delta;
			accumulatedDelta += delta;
			accumulatedDeltaMinCounter.Add(when, accumulatedDelta);

			//Deltas 
			int64_t accumulatedDeltaMin = accumulatedDeltaMinCounter.GetMin().value_or(0);
			int64_t acumulatedDeltaRelative = accumulatedDelta - accumulatedDeltaMin;
			//Get current estimated rtt
			int32_t rttMin = GetMinRTT();
			//Calculate estimated rtt
			int32_t rttEstimated = rttMin + acumulatedDeltaRelative /1000;
			
			//Dump stats
			//Log("recv #%u sent:%.8lu (+%.6ld) recv:%.8lu (+%.6ld) delta:%.6ld fb:%u, size:%u, bwe:%lu rtt:%lld rttMin:%lld acuDelta:%lld acuDeltaMin:%lld)\n",transportSeqNum,sent,deltaSent,recv,deltaRecv,delta,feedbackNum, stat->size, bandwidthEstimation, rttEstimated, rttMin, accumulatedDelta/1000, accumulatedDeltaMin/1000);
			
			//If dumping to file
			if (fd!=FD_INVALID)
			{
				char msg[1024];
				//Create log
				int len = snprintf(msg, 1024, "%.8lu|%u|%hhu|%u|%lu|%lu|%lu|%lu|%ld|%ld|%ld|%u|%u|%u|%u|%u|%d|%d|%d|%d|%d\n", fb, transportSeqNum, feedbackNum, stat->size, sent, recv, deltaSent, deltaRecv, delta, accumulatedDelta/1000, accumulatedDeltaMin/1000, GetEstimatedBitrate(), GetTargetBitrate(), GetAvailableBitrate(), rtt, rttMin, rttEstimated, stat->mark, stat->rtx, stat->probing, state);
				//Write it
				[[maybe_unused]] ssize_t written = write(fd,msg,len);
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

				//Update received
				packetsReceivedAcumulator.Update(sentTime, 1);
				packetsLostAcumulator.Update(sentTime);
			} else {
				//Update lost
				packetsReceivedAcumulator.Update(sentTime);
				packetsLostAcumulator.Update(sentTime, 1);
			}
		} else {
			//Log
			Warning("-SendSideBandwidthEstimation::ReceivedFeedback() | Packet not found [transportSeqNum:%u,receivedTime:%llu,first:%u,last:%u]\n", transportSeqNum, receivedTime, transportWideSentPacketsStats.GetFirstSeq(), transportWideSentPacketsStats.GetLastSeq());
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

uint32_t SendSideBandwidthEstimation::UpdateMinRTT(uint64_t when)
{
	//Update minimum
	rttMin.RollWindow(when);
	
	//Get new value
	return GetMinRTT();
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

uint32_t SendSideBandwidthEstimation::GetTotalSentBitrate() const
{
	return totalSentAcumulator.GetInstantAvg() * 8;
}

void SendSideBandwidthEstimation::SetState(ChangeState state)
{
	UltraDebug("-SendSideBandwidthEstimation::SetState() [state:%d,prev:%d,consecutiveChanges:%d]\n",state,this->state,consecutiveChanges);

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
	
	//Get loss rate
	double receivedPackets = packetsReceivedAcumulator.GetInstantAvg();
	double lostPackets = packetsLostAcumulator.GetInstantAvg();
	//Get loss rate
	double lossRate = lostPackets / (receivedPackets + lostPackets);

	//Get current estimated rtt and minimum delta
	int32_t rttMin			= UpdateMinRTT(when);
	int64_t accumulatedDeltaMin	= accumulatedDeltaMinCounter.Min(when).value_or(0);
	//Get relative delta
	int64_t delta = accumulatedDelta - accumulatedDeltaMin;
	//Calculate estimated rtt
	int32_t rttEstimated		= rttMin + delta/1000;
	
	//Update sent bitrates
	totalSentAcumulator.Update(when);
	mediaSentAcumulator.Update(when);
	rtxSentAcumulator.Update(when);

	//Get bitrates
	uint64_t totalRecvBitrate	= totalRecvAcumulator.GetInstantAvg() * 8;
	uint64_t totalSentBitrate	= totalSentAcumulator.GetInstantAvg() * 8;
	uint64_t mediaSentBitrate	= mediaSentAcumulator.GetInstantAvg() * 8;
	uint64_t rtxSentBitrate		= rtxSentAcumulator.GetInstantAvg() * 8;
	
	
	if (lossRate > LoosRateThreshold)
	{
		//We are on a loosy environment
		SetState(ChangeState::Loosy);
		//If there was any feedback loss
		if (totalRecvAcumulator.IsInWindow())
			//Set bwe as received rate
			bandwidthEstimation = totalRecvBitrate;
		//Increase target bitrate
		targetBitrate = std::max<uint32_t>(bandwidthEstimation, targetBitrate);
	} else if (rttEstimated>(10+rttMin*1.3)) {
		//We are in congestion
		SetState(ChangeState::Congestion);
		//If there was any feedback loss
		if (totalRecvAcumulator.IsInWindow())
			//Converge bwe as received rate
			bandwidthEstimation = bandwidthEstimation * 0.80 + totalRecvBitrate * 0.20;
		//Decrease target bitrate
		targetBitrate = std::min<uint32_t>(bandwidthEstimation, totalRecvBitrate);
	} else if (mediaSentBitrate > targetBitrate) {
		//We are overshooting
		SetState(ChangeState::OverShoot);
		//If there was any feedback loss
		if (totalRecvAcumulator.IsInWindow())
			//Take maximum of the spike and current value
			bandwidthEstimation = std::max<uint64_t>(bandwidthEstimation, totalRecvBitrate);
		//If there was enough data
		if (totalRecvAcumulator.IsInWindow())
			//Decrease target bitrate
			targetBitrate = std::min<uint32_t>(bandwidthEstimation, totalRecvBitrate);
	} else if (state == ChangeState::Initial) {
		//If first state 
		if (!lastChange)
			//Set to now
			lastChange = when;
		//Wait until we have  enought data
		else if (lastChange + kInitialDuration < when)
			//We are going to increase rate again
			SetState(ChangeState::Increase);
		else
			//Keep state
			SetState(ChangeState::Initial);

		//If there was enough data
		if (totalRecvAcumulator.IsInWindow())
			bandwidthEstimation = std::max(bandwidthEstimation, totalRecvBitrate);

		//Initial conversion factor
		double confidenceAmplifier = 1 + std::log(consecutiveChanges + 1);
		//Get rate change
		int64_t rateChange = std::max<uint64_t>(bandwidthEstimation * confidenceAmplifier * kInitialRampUp, kMinRateChangeBps);
		//Increase
		targetBitrate = std::min(targetBitrate, bandwidthEstimation) + rateChange;
	} else if  (state == ChangeState::OverShoot) {
		//Increase again
		SetState(ChangeState::Increase);
		//If bitrate is higher than bwe
		if (totalSentBitrate > bandwidthEstimation)
			//bwe to converge to target bitrate
			bandwidthEstimation = bandwidthEstimation * 0.90 + totalSentBitrate * 0.10;
	} else if (state==ChangeState::Congestion || state == ChangeState::Loosy) {
		//We are going to conservatively reach the previous estimation again
		SetState(ChangeState::Recovery);
	} else if (mediaSentBitrate > targetBitrate) {
		//We are overshooting
		SetState(ChangeState::OverShoot);
		//Take maximum of the spike and current value
		bandwidthEstimation = std::max<uint64_t>(bandwidthEstimation, totalRecvBitrate);
		//Set target to currenct received rate
		targetBitrate = totalRecvBitrate;
	} else if (state == ChangeState::Recovery) {
		//If there was enough data
		if (totalRecvAcumulator.IsInWindow())
			//Decrease bwe to converge to the received rate
			bandwidthEstimation = bandwidthEstimation * 0.90 + totalRecvBitrate * 0.10;

		//Initial conversion factor
		double confidenceAmplifier = std::log(consecutiveChanges + 1);
		//Get rate change
		int64_t rateChange = std::max<uint64_t>(bandwidthEstimation * confidenceAmplifier * kSamplingStep, kMinRateChangeBps);

		//Increase the target rate
		targetBitrate = std::min(bandwidthEstimation, targetBitrate) + rateChange;

		//When we have reached the bwe
		if (targetBitrate >= bandwidthEstimation && lastFeedbackDelta < 1000 && consecutiveChanges > 1)
			//We are going to increase rate again
			SetState(ChangeState::Increase);
		else 
			//We are going to conservatively reach the previous estimation again
			SetState(ChangeState::Recovery);
	
	} else if (state==ChangeState::Increase) {
		//Keep state
		SetState(ChangeState::Increase);
		//Initial conversion factor
		double confidenceAmplifier = 1 + std::log(consecutiveChanges+1);
		//Get rate change
		int64_t rateChange = std::max<uint64_t>(totalRecvBitrate * confidenceAmplifier * kSamplingStep, kMinRateChangeBps);
		//If bitrate is higher than bwe
		if (totalSentBitrate > bandwidthEstimation)
			//bwe to converge to target bitrate
			bandwidthEstimation = bandwidthEstimation * 0.90 + totalSentBitrate * 0.10;
		//If there was enough data
		if (totalRecvAcumulator.IsInWindow())
			//Calcuate new estimation
			targetBitrate = std::max(bandwidthEstimation, totalRecvBitrate) + rateChange;
	}	 

	//If rtt increasing
	if (lastFeedbackDelta > 2000 && delta > 0)
	{
		[[maybe_unused]] auto prev = targetBitrate;
		//Decrease factor
		double factor = 1 - static_cast<double>(delta) / (delta + rttEstimated * 1000 + kMonitorDuration);
		//Adapt to rtt slope, accumulatedDelta MUST be possitive
		targetBitrate = targetBitrate * factor;

		//Log("lastFeedbackDelta:%d delta:%d accumulatedDeltaMin:%d accumulatedDelta:%d bwe:%lld,target:%lld,new:%lld,factor:%f\n", lastFeedbackDelta, delta, accumulatedDeltaMin, accumulatedDelta, bandwidthEstimation, prev, targetBitrate, factor);
	} 

	//Calculate term rtx overhead
	double overhead = mediaSentBitrate ? static_cast<double>(mediaSentBitrate) / (mediaSentBitrate + rtxSentBitrate) : 1.0f;

	//Available rate taking into account current rtx overhead
	availableRate = targetBitrate * overhead; 

	//Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [this:%p,estimate:%llubps,target:%llubps,available:%llubps,sent:%llubps,recv:%llubps,rtx=%llubps,state:%d,delta=%d,acuDelta:%d,aduDeltaMin:%d,media:%u,rtx:%d,overhead:%.2f,rttEstimated:%d,rttMin:%d,received:%f,lost:%f,lossRate:%f\n",
	//	this,
	//	bandwidthEstimation,
	//	targetBitrate,
	//	availableRate,
	//	totalSentBitrate,
	//	totalRecvBitrate,
	//	rtxSentBitrate,
	//	state,
	//	static_cast<int32_t>(delta/1000),
	//	static_cast<int32_t>(accumulatedDelta/1000),
	//	static_cast<int32_t>(accumulatedDeltaMin/1000),
	//	mediaSentAcumulator.GetAcumulated(),
	//	rtxSentAcumulator.GetAcumulated(),
	//	(1-overhead),
	//	rttEstimated,
	//	rttMin,
	//	receivedPackets,
	//	lostPackets,
	//	lossRate
	//);


	//Set min/max limits
	bandwidthEstimation = std::min(std::max(bandwidthEstimation,kMinRate), kMaxRate);
	targetBitrate = std::min(std::max(targetBitrate, kMinRate), kMaxRate);
	availableRate = std::min(std::max(availableRate, kMinRate), kMaxRate);

	//Check we have listener
	if (!listener)
		return;
	
	//Check when we have to trigger a new bwe change on the app
	if (state != ChangeState::Initial && (((state == ChangeState::Congestion || state == ChangeState::Loosy) && consecutiveChanges==0) || ( lastChange + kReportInterval < when)))
	{
		//Log("-SendSideBandwidthEstimation::EstimateBandwidthRate() [estimate:%llubps,target:%llubps,available:%llubps,sent:%llubps,recv:%llubps,rtx=%llubps,state:%d,delta=%d,media:%u,rtx:%d,overhead:%f,when:%llu,diff:%llu\n",
		//	bandwidthEstimation,
		//	targetBitrate,
		//	availableRate,
		//	totalSentBitrate,
		//	totalRecvBitrate,
		//	rtxSentBitrate,
		//	state,
		//	static_cast<int32_t>(delta/1000),
		//	mediaSentAcumulator.GetAcumulated(),
		//	rtxSentAcumulator.GetAcumulated(),
		//	(1-overhead),
		//	when,
		//	when - lastChange
		//);
		//Call listener	
		listener->onTargetBitrateRequested(availableRate, bandwidthEstimation, totalSentBitrate);
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
