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
AudioCodec::Type GetRtmpFrameAudioCodec(const RTMPAudioFrame& audioFrame);

class RTMPAudioPacketizer
{
public:
	RTMPAudioPacketizer(AudioCodec::Type type) :
		type(type)
	{}
	AudioCodec::Type GetCodec() const { return type; }
	virtual std::unique_ptr<AudioFrame> AddFrame(RTMPAudioFrame* videoFrame) = 0;
private:
	AudioCodec::Type type;
};

class RTMPVideoPacketizer
{
public:
	RTMPVideoPacketizer(VideoCodec::Type type) :
		type(type)
	{}
	VideoCodec::Type GetCodec() const { return type; }
	virtual std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame) = 0;
private:
	VideoCodec::Type type;
};

std::unique_ptr<RTMPVideoPacketizer> CreateRTMPVideoPacketizer(VideoCodec::Type type);
std::unique_ptr<RTMPAudioPacketizer> CreateRTMPAudioPacketizer(AudioCodec::Type type);


template<typename DescClass, VideoCodec::Type codec>
class RTMPVideoPacketizerImpl : public RTMPVideoPacketizer
{
public:
	RTMPVideoPacketizerImpl() :
		RTMPVideoPacketizer(codec)
	{}
protected:
	
	std::unique_ptr<VideoFrame> PrepareFrame(RTMPVideoFrame* videoFrame);
	
	DescClass desc;
	bool gotConfig = false;
};


template<typename DescClass, typename SPSClass, typename PPSClass, VideoCodec::Type codec>
class RTMPH26xPacketizer : public RTMPVideoPacketizerImpl<DescClass, codec>
{
public:
	virtual std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame) override;
	
	std::optional<SPSClass> sps;
	std::optional<PPSClass> pps;
};

using RTMPAVCPacketizer = RTMPH26xPacketizer<AVCDescriptor, H264SeqParameterSet, H264PictureParameterSet, VideoCodec::H264>;
using RTMPHEVCPacketizer = RTMPH26xPacketizer<HEVCDescriptor, H265SeqParameterSet, H265PictureParameterSet, VideoCodec::H265>;

class RTMPAv1Packetizer : public RTMPVideoPacketizerImpl<AV1CodecConfigurationRecord, VideoCodec::AV1>
{
public:
	virtual std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame) override;
};


class RTMPAACPacketizer : public RTMPAudioPacketizer
{
public: 
	RTMPAACPacketizer() : RTMPAudioPacketizer(AudioCodec::AAC) {}
	virtual std::unique_ptr<AudioFrame> AddFrame(RTMPAudioFrame* videoFrame) override;
private:
	AACSpecificConfig aacSpecificConfig;
        bool gotConfig = false;
};


class RTMPG711APacketizer : public RTMPAudioPacketizer
{
public:
	RTMPG711APacketizer() : RTMPAudioPacketizer(AudioCodec::PCMA) {}
	virtual std::unique_ptr<AudioFrame> AddFrame(RTMPAudioFrame* videoFrame) override;
};

class RTMPG711UPacketizer : public RTMPAudioPacketizer
{
public:
	RTMPG711UPacketizer() : RTMPAudioPacketizer(AudioCodec::PCMU) { }
	virtual std::unique_ptr<AudioFrame> AddFrame(RTMPAudioFrame* videoFrame) override;
};

#endif /* RTMPPACKETIZER_H */

