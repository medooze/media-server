#ifndef RTMPPACKETIZER_H
#define RTMPPACKETIZER_H

#include "rtmp/rtmp.h"
#include "rtmp/rtmpmessage.h"
#include "media.h"
#include "audio.h"
#include "video.h"
#include <memory>


class RTMPAVCPacketizer
{
public:
	std::unique_ptr<VideoFrame> AddFrame(RTMPVideoFrame* videoFrame);
private:
	AVCDescriptor desc;
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

