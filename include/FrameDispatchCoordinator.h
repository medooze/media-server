#ifndef FRAMEDISPATCHCOORDINATOR_H
#define FRAMEDISPATCHCOORDINATOR_H

#include "media.h"
#include "rtp/RTPPacket.h"
#include "MediaFrameListenerBridge.h"
#include <chrono>
#include <queue>

class FrameDispatchCoordinator
{
public:
	static constexpr int DefaultUpdateRefsPacketEarlyThresholdMs = -200;
	static constexpr int DefaultUpdateRefsPacketLateThresholdMs = 0;	
	static constexpr std::chrono::milliseconds DefaultUpdateRefsStepPacketEarlyMs = std::chrono::milliseconds(20);

	FrameDispatchCoordinator(int aUpdateRefsPacketEarlyThresholdMs = DefaultUpdateRefsPacketEarlyThresholdMs,
				int aUpdateRefsPacketLateThresholdMs = DefaultUpdateRefsPacketLateThresholdMs, 
				std::chrono::milliseconds aUpdateRefsStepPacketEarlyMs = DefaultUpdateRefsStepPacketEarlyMs);
	
	void OnFrame(std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate, MediaFrameListenerBridge& listenerBridge);

private:
	
	/**
	 * Get the delay of current frame
	 * 
	 * @param now The current time
	 * @param unifiedTs The unified timestamp
	 * 
	 * @return The delay of current frame, which is value of current time minus scheduled time for the packet, in milliseconds.
	 */
	int64_t GetFrameArrivalDelayMs(std::chrono::milliseconds now, uint64_t unifiedTs) const;
	
	// The following thresholds are checking the offsets of arrival time with regard to the previously
	// scheduled time.
	
	// Controls the stability of the dispatching.
	int updateRefsPacketEarlyThresholdMs = 0;	
	// Controls how accurate the a/v sync. Smaller means more in sync. Must >= 0. 
	int updateRefsPacketLateThresholdMs = 0;
	// Controls the latency reduction speed
	std::chrono::milliseconds updateRefsStepPacketEarlyMs {0};
	 
	bool initialized = false;
	bool reducingLatency = false;
	
	std::chrono::milliseconds refTime {0};
	uint64_t refTimestamp = 0;
	
	std::unordered_map<MediaFrameListenerBridge*, std::pair<std::chrono::milliseconds, uint64_t>> frameArrivalInfo;		
	mutable std::mutex mutex;
	
	friend class TestFrameDispatchCoordinator;
};

#endif
