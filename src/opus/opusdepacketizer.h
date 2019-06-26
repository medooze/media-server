
#ifndef OPUSDEPACKETIZER_H
#define OPUSDEPACKETIZER_H

#include "opusconfig.h"
#include "audio.h"
#include "rtp/RTPDepacketizer.h"

class OpusDepacketizer : public RTPDepacketizer
{
public:
	OpusDepacketizer() :
		RTPDepacketizer(MediaFrame::Audio,AudioCodec::OPUS),
		frame(AudioCodec::OPUS)
	{
		frame.SetClockRate(48000);
	}

	virtual ~OpusDepacketizer() = default;

	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload,DWORD payload_len) override;
	virtual void ResetFrame() override;
	
	
private:
	AudioFrame frame;
	OpusConfig config;
};


#endif /* OPUSDEPACKETIZER_H */

