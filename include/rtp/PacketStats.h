#ifndef PACKETSTATS_H
#define PACKETSTATS_H

#include "rtp/RTPPacket.h"

struct PacketStats
{
	static PacketStats Create(const RTPPacket::shared& packet, uint32_t size, uint64_t now)
	{
		return PacketStats {
			.transportWideSeqNum	= packet->GetTransportSeqNum(),
			.ssrc			= packet->GetSSRC(),
			.extSeqNum		= packet->GetExtSeqNum(),
			.size			= size,
			.payload		= packet->GetMediaLength(),
			.timestamp		= packet->GetTimestamp(),
			.time			= now,
			.mark			= packet->GetMark(),
		};
	}

	static PacketStats CreateRTX(const RTPPacket::shared& packet, uint32_t size, uint64_t now)
	{
		//Create stat
		auto stats = Create(packet, size, now);
		stats.rtx = true;
		return stats;
	}

	static PacketStats CreateProbing(const RTPPacket::shared& packet, uint32_t size, uint64_t now)
	{
		//Create stat
		auto stats = Create(packet, size, now);
		stats.probing = true;
		return stats;
	}

	
	static PacketStats Create(uint32_t transportWideSeqNum, uint32_t ssrc,uint32_t extSeqNum, uint32_t size, uint32_t payload, uint32_t timestamp, uint64_t now, bool mark)
	{
		return PacketStats {
			.transportWideSeqNum	= transportWideSeqNum,
			.ssrc			= ssrc,
			.extSeqNum		= extSeqNum,
			.size			= size,
			.payload		= payload,
			.timestamp		= timestamp,
			.time			= now,
			.mark			= mark,
		};
	}

	static PacketStats CreateRTX(uint32_t transportWideSeqNum, uint32_t ssrc, uint32_t extSeqNum, uint32_t size, uint32_t payload, uint32_t timestamp, uint64_t now, bool mark)
	{
		//Create stat
		auto stats = Create(transportWideSeqNum, ssrc, extSeqNum, size, payload, timestamp, now, mark);
		stats.rtx = true;
		return stats;
	}

	static PacketStats CreateProbing(uint32_t transportWideSeqNum, uint32_t ssrc, uint32_t extSeqNum, uint32_t size, uint32_t payload, uint32_t timestamp, uint64_t now, bool mark)
	{
		//Create stat
		auto stats = Create(transportWideSeqNum, ssrc, extSeqNum, size, payload, timestamp, now, mark);
		stats.probing = true;
		return stats;
	}

	uint32_t transportWideSeqNum;
	uint32_t ssrc;
	uint32_t extSeqNum;
	uint32_t size;
	uint32_t payload;
	uint32_t timestamp;
	uint64_t time;
	bool  mark	= false;
	bool  rtx	= false;
	bool  probing	= false;
	uint32_t bwe	= 0;
};

#endif /* PACKETSTATS_H */

