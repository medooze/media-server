#ifndef FECPROBEGENERATOR_H
#define	FECPROBEGENERATOR_H

#include "rtp/RTPMap.h"
#include "rtp/RTPPacket.h"
#include "CircularQueue.h"
#include "ForwardErrorCorrection.h"

class FecProbeGenerator
{
public:
	FecProbeGenerator(const CircularQueue<RTPPacket::shared>& history, const RTPMap& extMap) :
		history(history),
		extMap(extMap) {};

	std::vector<RTPPacket::shared> PrepareFecProbePackets(
		WORD fecRate,
		bool produceAtLeastOnePacket);
	std::optional<DWORD> GetLastProtectedSsrc() { return lastSsrc; }

private:
	std::optional<DWORD> lastSsrc;
	std::optional<DWORD> lastExtSeqNum;
	ForwardErrorCorrection fec;
	const CircularQueue<RTPPacket::shared>& history;
	const RTPMap& extMap;
};

#endif /* FECPROBEGENERATOR_H */
