#ifndef RTMPPACKETIZER_H
#define RTMPPACKETIZER_H

#include "rtmp/rtmp.h"
#include "rtmp/rtmpmessage.h"
#include "media.h"
#include "audio.h"
#include "video.h"
#include "h264/h264.h"
#include <memory>

VideoCodec::Type GetRtmpFrameVideoCodec(const RTMPVideoFrame& videoFrame);

template<typename DescClass, typename SPSClass, VideoCodec::Type codec>
class RTMPH26xPacketizer
{
public:
	std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame);

private:
	
	DescClass desc;
	bool gotConfig = false;
};

using RTMPAVCPacketizer = RTMPH26xPacketizer<AVCDescriptor, H264SeqParameterSet, VideoCodec::H264>;
using RTMPHEVCPacketizer = RTMPH26xPacketizer<HEVCDescriptor, H265SeqParameterSet, VideoCodec::H265>;

class RTMPAACPacketizer
{
public:
	std::unique_ptr<AudioFrame> AddFrame(RTMPAudioFrame* videoFrame);
private:
	AACSpecificConfig aacSpecificConfig;
        bool gotConfig = false;
};

#endif /* RTMPPACKETIZER_H */

