/* 
 * File:   h264depacketizer.h
 * Author: Sergio
 *
 * Created on 26 de enero de 2012, 9:46
 */

#ifndef H264DEPACKETIZER_H
#define	H264DEPACKETIZER_H
#include "rtp.h"
#include "video.h"
#include "avcdescriptor.h"

class H264Depacketizer : public RTPDepacketizer
{
public:
	H264Depacketizer(bool annexB = false);
	virtual ~H264Depacketizer();
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload,DWORD payload_len) override;
	virtual void ResetFrame() override;
private:
	VideoFrame frame;
	AVCDescriptor config;
	DWORD iniFragNALU = 0;
	bool startedFrag = false;
	bool annexB = false;
};

#endif	/* H264DEPACKETIZER_H */

