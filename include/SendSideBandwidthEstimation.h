#ifndef SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <deque>
#include <utility>
#include <vector>

#include "acumulator.h"
#include "MovingCounter.h"
#include "rtp/PacketStats.h"
#include "remoterateestimator.h"
#include "CircularBuffer.h"
#include "WrapExtender.h"

class SendSideBandwidthEstimation
{
	
public:
	enum ChangeState {
		Initial,
		Increase,
		OverShoot,
		Congestion,
		Recovery,
		Loosy
	}
;
public:
	SendSideBandwidthEstimation();
        ~SendSideBandwidthEstimation();
	void SentPacket(const PacketStats& packet);
	void ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets, uint64_t when = 0);
	void UpdateRTT(uint64_t when, uint32_t rtt);
	uint32_t GetEstimatedBitrate() const;
	uint32_t GetTargetBitrate() const;
	uint32_t GetAvailableBitrate() const;
	uint32_t GetTotalSentBitrate() const;
	uint32_t UpdateMinRTT(uint64_t when);
	uint32_t GetMinRTT() const;
	void SetListener(RemoteRateEstimator::Listener* listener) { this->listener = listener; }

        
        int Dump(const char* filename);
	int StopDump();
private:
	void SetState(ChangeState state);
	void EstimateBandwidthRate(uint64_t when);
private:
	class Stats
	{
	public:
		uint64_t time;
		uint32_t size;
		bool  mark = false;
		bool  rtx = false;
		bool  probing = false;
	};
private:
	CircularBuffer<Stats, uint16_t, 32768> transportWideSentPacketsStats;
	uint64_t bandwidthEstimation = 0;
	uint64_t targetBitrate = 0;
	uint64_t availableRate = 0;
	uint64_t firstSent = 0;
	uint64_t firstRecv = 0;
	uint64_t lastRecv = 0;
	uint64_t prevSent = 0;
	uint64_t prevRecv = 0;
        uint32_t rtt = 0;
        uint64_t lastChange = 0;
	int64_t  accumulatedDelta = 0;
	int64_t  lastFeedbackDelta = 0;
        int fd = FD_INVALID;
	
	ChangeState state = ChangeState::Initial;
	uint32_t consecutiveChanges = 0;
	
	WrapExtender<uint8_t, uint64_t> feedbackNumExtender;
	uint64_t lastFeedbackNum = 0;
	
	MovingMinCounter<int64_t> rttMin;
	MovingMinCounter<int64_t>  accumulatedDeltaMinCounter;
	Acumulator<uint32_t, uint64_t> totalSentAcumulator;
	Acumulator<uint32_t, uint64_t> mediaSentAcumulator;
	Acumulator<uint32_t, uint64_t> rtxSentAcumulator;
	Acumulator<uint32_t, uint64_t> probingSentAcumulator;
	Acumulator<uint32_t, uint64_t> totalRecvAcumulator;
	Acumulator<uint32_t, uint64_t> mediaRecvAcumulator;
	Acumulator<uint32_t, uint64_t> rtxRecvAcumulator;
	Acumulator<uint32_t, uint64_t> probingRecvAcumulator;
	Acumulator<uint32_t, uint64_t> packetsReceivedAcumulator;
	Acumulator<uint32_t, uint64_t> packetsLostAcumulator;
	

	
	RemoteRateEstimator::Listener* listener = nullptr;

};

#endif  // CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
