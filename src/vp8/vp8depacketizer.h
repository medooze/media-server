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
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload,DWORD payload_len) override;
	virtual void ResetFrame() override;
private:
	enum class State
	{
		None,
		Processing,
		Error
	};

	VideoFrame frame;
	uint32_t lastSeqNumber = 0;
	State state = State::None;
};

#endif	/* VP8DEPACKETIZER_H */
