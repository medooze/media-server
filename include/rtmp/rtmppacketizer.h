#ifndef RTMPPACKETIZER_H
#define RTMPPACKETIZER_H

#include "rtmp/rtmp.h"
#include "rtmp/rtmpmessage.h"
#include "media.h"
#include "audio.h"
#include "video.h"
#include "h264/h264.h"
#include <memory>

#include "RawFrameProcessor.h"
#include "FrameMediaAppender.h"


class RtmpVideoPacketizer : public RawFrameProcessor
{
public:
	RtmpVideoPacketizer(VideoCodec::Type codec, std::unique_ptr<CodecConfigurationRecord> codecConfig);

	virtual std::unique_ptr<MediaFrame> AddFrame(RawFrame* rawFrame) override;

	static VideoCodec::Type GetRtmpFrameVideoCodec(const RTMPVideoFrame& rawFrame);
	
private:
	
	VideoCodec::Type codec;
	
	std::unique_ptr<CodecConfigurationRecord> codecConfig;
	
	unsigned frameWidth = 0;
	unsigned frameHeight = 0;
	
	bool gotConfig = false;
};

class RTMPAACPacketizer
{
public:
	std::unique_ptr<AudioFrame> AddFrame(RTMPAudioFrame* videoFrame);
private:
	AACSpecificConfig aacSpecificConfig;
        bool gotConfig = false;
};

#endif /* RTMPPACKETIZER_H */

