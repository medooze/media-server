#ifndef RTMPPACKETIZER_H
#define RTMPPACKETIZER_H

#include "rtmp/rtmp.h"
#include "rtmp/rtmpmessage.h"
#include "media.h"
#include "audio.h"
#include "video.h"
#include "h264/h264.h"
#include "av1/AV1CodecConfigurationRecord.h"

#include <memory>

VideoCodec::Type GetRtmpFrameVideoCodec(const RTMPVideoFrame& videoFrame);

template<typename DescClass, VideoCodec::Type codec>
class RTMPPacketizer
{
public:
	virtual std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame) = 0;

protected:
	
	std::unique_ptr<VideoFrame> PrepareFrame(RTMPVideoFrame* videoFrame);
	
	DescClass desc;
	bool gotConfig = false;
};


template<typename DescClass, typename SPSClass, typename PPSClass, VideoCodec::Type codec>
class RTMPH26xPacketizer : public RTMPPacketizer<DescClass, codec>
{
public:
	virtual std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame) override;
	
	std::optional<SPSClass> sps;
	std::optional<PPSClass> pps;
};

using RTMPAVCPacketizer = RTMPH26xPacketizer<AVCDescriptor, H264SeqParameterSet, H264PictureParameterSet, VideoCodec::H264>;
using RTMPHEVCPacketizer = RTMPH26xPacketizer<HEVCDescriptor, H265SeqParameterSet, H265PictureParameterSet, VideoCodec::H265>;

class RTMPAv1Packetizer : public RTMPPacketizer<AV1CodecConfigurationRecord, VideoCodec::AV1>
{
public:
	virtual std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame) override;
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

