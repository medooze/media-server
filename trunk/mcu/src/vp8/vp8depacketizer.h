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
	DWORD iniFragNALU;
};

#endif	/* VP8DEPACKETIZER_H */

