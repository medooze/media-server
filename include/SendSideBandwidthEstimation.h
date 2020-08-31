#ifndef SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <deque>
#include <utility>
#include <vector>

#include "acumulator.h"
#include "rtp/PacketStats.h"
#include "remoterateestimator.h"

class SendSideBandwidthEstimation
{
	
public:
	enum ChangeState {
		Initial,
		Increase,
		Decrease,
		OverShoot,
		Congestion
	}
;
public:
	SendSideBandwidthEstimation();
        ~SendSideBandwidthEstimation();
	void SentPacket(const PacketStats::shared& packet);
	void ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets, uint64_t when = 0);
	void UpdateRTT(uint64_t when, uint32_t rtt);
	uint32_t GetEstimatedBitrate() const;
	uint32_t GetTargetBitrate() const;
	uint32_t GetAvailableBitrate() const;
	uint32_t GetMinRTT() const;
	void SetListener(RemoteRateEstimator::Listener* listener) { this->listener = listener; }

        
        int Dump(const char* filename);
	int StopDump();
private:
	void SetState(ChangeState state);
	void EstimateBandwidthRate(uint64_t when);
private:
	std::map<DWORD,PacketStats::shared> transportWideSentPacketsStats;
	uint64_t bandwidthEstimation = 0;
	uint64_t targetBitrate = 0;
	uint64_t availableRate = 0;
	uint64_t firstSent = 0;
	uint64_t lastSent = 0;
	uint64_t firstRecv = 0;
	uint64_t lastRecv = 0;
	uint64_t prevSent = 0;
	uint64_t prevRecv = 0;
        uint32_t rtt = 0;
        uint64_t lastChange = 0;
        int fd = FD_INVALID;
	
	int64_t accumulatedDelta = 0;
	
	ChangeState state = ChangeState::Initial;
	uint32_t consecutiveChanges = 0;
	

	
	Acumulator rttAcumulator;
	Acumulator deltaAcumulator;
	Acumulator totalSentAcumulator;
	Acumulator mediaSentAcumulator;
	Acumulator rtxSentAcumulator;
	Acumulator probingSentAcumulator;
	Acumulator totalRecvAcumulator;
	Acumulator mediaRecvAcumulator;
	Acumulator rtxRecvAcumulator;
	Acumulator probingRecvAcumulator;
	
	RemoteRateEstimator::Listener* listener = nullptr;

};

#endif  // CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
