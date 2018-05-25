/* 
 * File:   vp8depacketizer.h
 * Author: Sergio
 *
 * Created on 26 de enero de 2012, 9:46
 */

#ifndef VP8DEPACKETIZER_H
#define	VP8DEPACKETIZER_H
#include "rtp.h"
#include "video.h"

class VP8Depacketizer : public RTPDepacketizer
{
public:
	VP8Depacketizer();
	virtual ~VP8Depacketizer();
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

#endif	/* VP8DEPACKETIZER_H */

