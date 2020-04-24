#ifndef VP9DEPACKETIZER_H
#define	VP9DEPACKETIZER_H
#include "rtp.h"
#include "video.h"

class VP9Depacketizer : public RTPDepacketizer
{
public:
	VP9Depacketizer();
	virtual ~VP9Depacketizer();
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload,DWORD payload_len) override;
	virtual void ResetFrame() override;
private:
	VideoFrame frame;
	LayerFrame layer;
};

#endif	/* VP9DEPACKETIZER_H */

