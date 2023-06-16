
#ifndef H265DEPACKETIZER_H
#define	H265DEPACKETIZER_H
#include "rtp.h"
#include "video.h"
#include "HEVCdescriptor.h"

class H265Depacketizer : public RTPDepacketizer
{
public:
	H265Depacketizer();
	virtual ~H265Depacketizer();
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload, DWORD payload_len) override;
	virtual void ResetFrame() override;
private:
	VideoFrame frame;
	HEVCDescriptor config;
	DWORD iniFragNALU = 0;
	bool startedFrag = false;
};

#endif	/* H265DEPACKETIZER_H */

