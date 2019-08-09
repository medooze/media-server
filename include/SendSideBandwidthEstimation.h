#ifndef SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <deque>
#include <utility>
#include <vector>

#include "rtp/PacketStats.h"
#include "remoterateestimator.h"

class SendSideBandwidthEstimation
{
public:
	SendSideBandwidthEstimation();
        ~SendSideBandwidthEstimation();
	void SentPacket(const PacketStats::shared& packet);
	void ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets, uint64_t when = 0);
	void UpdateRTT(uint32_t rtt);
	uint32_t GetEstimatedBitrate();
	
	void SetListener(RemoteRateEstimator::Listener* listener) { estimator.SetListener(listener); }
        
        int Dump(const char* filename);

private:
	std::map<DWORD,PacketStats::shared> transportWideSentPacketsStats;
	RemoteRateEstimator estimator;
	uint64_t firstSent = 0;
	uint64_t lastSent = 0;
	uint64_t firstRecv = 0;
	uint64_t lastRecv = 0;
	uint64_t prevSent = 0;
	uint64_t prevRecv = 0;
        uint32_t rtt = 0;
        
        int fd = FD_INVALID;
        
	
};

#endif  // CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
