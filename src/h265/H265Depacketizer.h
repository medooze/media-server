
#ifndef H265DEPACKETIZER_H
#define	H265DEPACKETIZER_H
#include "rtp.h"
#include "video.h"
#include "HEVCDescriptor.h"

class H265Depacketizer : public RTPDepacketizer
{
public:
	H265Depacketizer(bool annexB = false);
	virtual ~H265Depacketizer();
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload, DWORD payload_len) override;
	virtual void ResetFrame() override;
private:
	void AddCodecConfig();
	bool AddSingleNalUnitPayload(const BYTE* nalUnit, DWORD nalSize /*include nalHeader*/);

	VideoFrame frame;
	HEVCDescriptor config;
	DWORD iniFragNALU = 0;
	bool startedFrag = false;
	bool annexB = false;
};

#endif	/* H265DEPACKETIZER_H */

