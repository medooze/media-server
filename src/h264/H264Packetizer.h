#ifndef H264PACKETIZER_H
#define H264PACKETIZER_H

#include "H26xPacketizer.h"
#include "avcdescriptor.h"

/**
 * RTP packetizer for H.264 streams.
 * 
 * packetization involves undoing incoming NAL stream (slicing NALUs, ignoring AUDs),
 * and retaining SPS/PPS to then inject on each keyframe as required by WebRTC.
 */
class H264Packetizer : public H26xPacketizer
{
public:
	H264Packetizer();
	std::unique_ptr<MediaFrame> ProcessAU(BufferReader &reader) override;

protected:
	void OnNal(VideoFrame& frame, BufferReader& nal, std::optional<bool>& frameEnd) override;
	void EmitNal(VideoFrame& frame, BufferReader nal);
	AVCDescriptor config;
	bool noPPSInFrame = true;
	bool noSPSInFrame = true;
	
	std::unique_ptr<H264SeqParameterSet> sps;
};

#endif // H264PACKETIZER_H
