#ifndef SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <deque>
#include <utility>
#include <vector>

#include "rtp/PacketStats.h"
#include "remoterateestimator.h"

class MonitorInterval
{
public:
	MonitorInterval(uint64_t target, uint64_t start,uint64_t duration) :
		target(target),
		start(start),
		duration(duration)
	{}
	MonitorInterval(uint64_t start,uint64_t duration) :
		start(start),
		duration(duration)
	{}
	
	bool SentPacket(uint64_t sent, uint64_t size);
	bool Feedback(uint64_t sent, uint64_t recv, uint64_t size, int64_t delta);
	
	uint64_t GetStartTime()		const { return start;			}
	uint64_t GetEndTime()		const { return start + duration;	}
	bool IsFeedbackCollectionDone() const { return feedbackCollectionDone;	}
	uint64_t GetTargetBitrate()	const { return target;			}
	uint64_t GetReceivedBitrate() const;
	uint64_t GetSentBitrate() const;
	double   GetLossRate() const;
	double	 ComputeDelayGradient() const;
	double   ComputeVivaceUtilityFunction() const;
	void	 Dump() const;
	
private:
	uint64_t start;
	uint64_t duration;
	uint64_t target				= 0;
	uint64_t firstSent			= std::numeric_limits<uint64_t>::max();
	uint64_t lastSent			= std::numeric_limits<uint64_t>::max();
	uint64_t firstRecv			= std::numeric_limits<uint64_t>::max();
	uint64_t lastRecv			= std::numeric_limits<uint64_t>::max();
	uint64_t accumulatedSentSize		= 0;
	uint64_t accumulatedReceivedSize	= 0;
	uint32_t lostPackets			= 0;
	uint32_t totalFeedbackedPackets		= 0;
	uint32_t totalSentPackets		= 0;
	bool feedbackCollectionDone		= 0;
	std::vector<std::pair<uint64_t,int64_t>> deltas;
};

class SendSideBandwidthEstimation
{
	
public:
	enum ChangeState {
		Maintain,
		Increase,
		Decrease,
		OverShoot,
	};
public:
	SendSideBandwidthEstimation();
        ~SendSideBandwidthEstimation();
	void SentPacket(const PacketStats::shared& packet);
	void ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets, uint64_t when = 0);
	void UpdateRTT(uint32_t rtt);
	uint32_t GetEstimatedBitrate() const;
	uint32_t GetTargetBitrate() const;
	
	void SetListener(RemoteRateEstimator::Listener* listener) { this->listener = listener; }
        
        int Dump(const char* filename);
private:
	void CreateIntervals(uint64_t time);
	void EstimateBandwidthRate();
private:
	std::map<DWORD,PacketStats::shared> transportWideSentPacketsStats;
	uint64_t bandwidthEstimation = 0;
	uint64_t availableRate = 0;
	uint64_t firstSent = 0;
	uint64_t lastSent = 0;
	uint64_t firstRecv = 0;
	uint64_t lastRecv = 0;
	uint64_t prevSent = 0;
	uint64_t prevRecv = 0;
        uint32_t rtt = 0;
        
        int fd = FD_INVALID;
	
	//RttTracker rttTracker;
	ChangeState state;
	uint32_t consecutiveChanges = 0;
	std::vector<MonitorInterval> monitorIntervals;
	
	RemoteRateEstimator::Listener* listener = nullptr;
        
	
};

#endif  // CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
