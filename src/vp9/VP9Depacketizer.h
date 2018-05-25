#ifndef VP9DEPACKETIZER_H
#define	VP9DEPACKETIZER_H
#include "rtp.h"
#include "video.h"

class VP9Depacketizer : public RTPDepacketizer
{
public:
	VP9Depacketizer();
	virtual ~VP9Depacketizer();
	virtual void SetTimestamp(DWORD timestamp) override;
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len) override;
	virtual void ResetFrame() override;
	virtual DWORD GetTimestamp() override
	{
		return frame.GetTimeStamp();
	} 
private:
	VideoFrame frame;
};

#endif	/* VP9DEPACKETIZER_H */

