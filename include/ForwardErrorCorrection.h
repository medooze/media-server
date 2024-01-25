#ifndef FORWARDERRORCORRECTION_H
#define	FORWARDERRORCORRECTION_H

#include <vector>
#include "rtp/RTPMap.h"
#include "rtp/RTPPacket.h"


class ForwardErrorCorrection
{
public:
	ForwardErrorCorrection();
	RTPPacket::shared EncodeFec(
		const std::vector<RTPPacket::shared>& mediaPackets,
		const RTPMap& extMap);


	BYTE MaxAllowedMaskSize() { return maxAllowedMaskSize; }
	void SetMaxAllowedMaskSize(BYTE maxAllowedMaskSize)
	{
		this->maxAllowedMaskSize = maxAllowedMaskSize;
	}
private:
	BYTE maxAllowedMaskSize;
	std::vector<BYTE> mask;
	std::vector<BYTE> buffer;
};

#endif /* FORWARDERRORCORRECTION_H */
