#ifndef H265PACKETIZER_H
#define H265PACKETIZER_H

#include "video.h"
#include "h265/HEVCDescriptor.h"

/**
 * RTP packetizer for H.265 streams.
 * 
 * packetization involves undoing Annex B byte stream (slicing NALUs, ignoring AUDs),
 * and retaining VPS/SPS/PPS to then inject on each keyframe as required by WebRTC.
 */
class H265Packetizer
{
public:
	std::unique_ptr<VideoFrame> ProcessAU(BufferReader payload);

	// this is not private because MPEGTSHEVCStream needs to access it
	// to know when to start allowing frames through
	HEVCDescriptor config;
private:
	uint32_t width = 0;
	uint32_t height = 0;
};

#endif // H265PACKETIZER_H
