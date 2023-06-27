#ifndef PACKETDISPATCHCOORDINATOR_H
#define PACKETDISPATCHCOORDINATOR_H

#include "media.h"
#include "rtp.h"
#include "TimeService.h"

class PacketDispatchCoordinator
{
public:
	/**
	 * Update internal time info for a newly arrived frame.
	 * 
	 * @param type 	The media type
	 * @param now 	The arrival time
	 * @param ts 	The frame timestamp
	 * @param clockRate The clock rate
	 */
	virtual void OnFrameArrival(MediaFrame::Type type, std::chrono::milliseconds now, uint64_t ts, uint64_t clockRate) = 0;

	/**
	 * Update internal time info for a generated packet.
	 * 
	 * @param type 	The media type
	 * @param nowUs	The packet time, in microseconds.
	 * @param ts 	The frame timestamp
	 * @param clockRate The clock rate
	 */
	virtual void OnPacket(MediaFrame::Type type, uint64_t nowUs, uint64_t ts, uint64_t clockRate) = 0;
	
	/**
	 * Get the packets for sending
	 * 
	 * @param packets The queue of all current pending packets. Upon output, the packets to send would be removed from the queue.
	 * @param period The period since last sending
	 * @param now The current time
	 * 
	 * @return A pair. The first element is the packets to be sent. The second element is the dispatch offset, which is the
	 *         value of current time minus scheduled time for next pending packet, in milliseconds.
	 */
	virtual std::pair<std::vector<RTPPacket::shared>, int64_t> GetPacketsToDispatch(std::queue<std::pair<RTPPacket::shared, std::chrono::milliseconds>>& packets, 
											std::chrono::milliseconds period,
											std::chrono::milliseconds now) const = 0; 
		
	/**
	 * Gets whether it should always dispatch previous packets when a new frame arrives.
	 */
	virtual bool AlwaysDispatchPreviousFramePackets() const = 0;
	
	/**
	 * Reset the internal state.
	 */
	virtual void Reset() = 0;
	
	/**
	 * Convert timestamp from one clock rate to another
	 * 
	 * @param ts The input timestamp
	 * @param originalRate The clock rate of the input timestamp
	 * @param targetRate The target clock rate
	 * 
	 * @return The timestamp basing on the target clock rate
	 */
	template<typename T>
	static constexpr T convertTimestampClockRate(T ts, uint64_t originalRate, uint64_t targetRate)
	{
		return originalRate == targetRate ? ts : (ts * T(targetRate) / T(originalRate));
	}
};

#endif
