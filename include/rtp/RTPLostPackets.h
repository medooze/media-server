#ifndef RTPLOSTPACKETS_H
#define RTPLOSTPACKETS_H

#include <list>

#include "config.h"
#include "rtp/RTPPacket.h"
#include "rtp/RTCPRTPFeedback.h"


class RTPLostPackets
{
public:
	RTPLostPackets(WORD num);
	~RTPLostPackets();
	void Reset();
	WORD AddPacket(const RTPPacket::shared &packet);
	std::list<RTCPRTPFeedback::NACKField::shared>  GetNacks() const;
	void Dump() const;
	DWORD GetTotal() const {return total;}
	
private:
	QWORD *packets;
	WORD size;
	WORD len;
	DWORD first;
	DWORD total;
};


#endif /* RTPLOSTPACKETS_H */

