#ifndef VP9DEPACKETIZER_H
#define	VP9DEPACKETIZER_H
#include "rtp.h"
#include "video.h"

class VP9Depacketizer : public RTPDepacketizer
{
public:
	VP9Depacketizer();
	virtual ~VP9Depacketizer();
	virtual void SetTimestamp(DWORD timestamp);
	virtual MediaFrame* AddPacket(RTPPacket *packet);
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len);
	virtual void ResetFrame();
	virtual DWORD GetTimestamp() 
	{
		return frame.GetTimeStamp();
	}
private:
	VideoFrame frame;
};

#endif	/* VP9DEPACKETIZER_H */

