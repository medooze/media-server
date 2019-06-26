#ifndef PACKETSTATS_H
#define PACKETSTATS_H

#include "rtp/RTPPacket.h"

struct PacketStats
{
	using shared = std::shared_ptr<PacketStats>;

	static PacketStats::shared Create(const RTPPacket::shared& packet, uint32_t size, uint64_t now)
	{
		auto stats = std::make_shared<PacketStats>();

		stats->transportWideSeqNum	= packet->GetTransportSeqNum();
		stats->ssrc			= packet->GetSSRC();
		stats->extSeqNum		= packet->GetExtSeqNum();
		stats->size			= size;
		stats->payload			= packet->GetMediaLength();
		stats->timestamp		= packet->GetTimestamp();
		stats->time			= now;
		stats->mark			= packet->GetMark();

		return stats;
	}
	
	static PacketStats::shared Create(uint32_t transportWideSeqNum, uint32_t ssrc,uint32_t extSeqNum, uint32_t size, uint32_t payload, uint32_t timestamp, uint64_t now, bool mark)
	{
		//Create stat
		auto stats = std::make_shared<PacketStats>();
		//Fill
		stats->transportWideSeqNum	= transportWideSeqNum;
		stats->ssrc			= ssrc;
		stats->extSeqNum		= extSeqNum;
		stats->size			= size;
		stats->payload			= payload;
		stats->timestamp		= timestamp;
		stats->time			= now;
		stats->mark			= mark;

		return stats;
	}

	uint32_t transportWideSeqNum;
	uint32_t ssrc;
	uint32_t extSeqNum;
	uint32_t size;
	uint32_t payload;
	uint32_t timestamp;
	uint64_t time;
	bool  mark;
};

#endif /* PACKETSTATS_H */

