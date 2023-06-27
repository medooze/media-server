#ifndef H264PACKETIZER_H
#define H264PACKETIZER_H

#include "video.h"
#include "avcdescriptor.h"

/**
 * RTP packetizer for H.264 streams.
 * 
 * packetization involves undoing Annex B byte stream (slicing NALUs, ignoring AUDs),
 * and retaining SPS/PPS to then inject on each keyframe as required by WebRTC.
 * it also drops frames until the first SPS/PPS are received.
 */
class H264Packetizer
{
public:
	std::unique_ptr<VideoFrame> ProcessAU(BufferReader payload);

	// this is not private because MPEGTSAVCStream needs to access it
	// to know when to start allowing frames through
	AVCDescriptor config;
private:
	uint32_t width = 0;
	uint32_t height = 0;
};

#endif // H264PACKETIZER_H
