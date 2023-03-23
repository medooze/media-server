
#ifndef H265DEPACKETIZER_H
#define	H265DEPACKETIZER_H
#include "rtp.h"
#include "video.h"

class H265Depacketizer : public RTPDepacketizer
{
public:
	H265Depacketizer();
	virtual ~H265Depacketizer();
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload, DWORD payload_len) override;
	virtual void ResetFrame() override;
	virtual void FinalizeFrame() override {};
private:
	VideoFrame frame;
	DWORD iniFragNALU = 0;
	bool startedFrag = false;
};

#endif	/* H265DEPACKETIZER_H */
