#ifndef H265PACKETIZER_H
#define H265PACKETIZER_H

#include "h264/H26xPacketizer.h"
#include "h265/HEVCDescriptor.h"

/**
 * RTP packetizer for H.265 streams.
 * 
 * packetization involves undoing incoming NAL stream (slicing NALUs, ignoring AUDs),
 * and retaining VPS/SPS/PPS to then inject on each keyframe as required by WebRTC.
 */
class H265Packetizer : public H26xPacketizer
{
public:
	H265Packetizer();
	std::unique_ptr<MediaFrame> ProcessAU(BufferReader& reader) override;

protected:
	void OnNal(VideoFrame& frame, BufferReader& nal, std::optional<bool>& frameEnd) override;

	void EmitNal(VideoFrame& frame, BufferReader nal);
	
	HEVCDescriptor config;
	bool noPPSInFrame = true;
	bool noSPSInFrame = true;
	bool noVPSInFrame = true;
	
	std::unique_ptr<H265PictureParameterSet> pps;
	std::unique_ptr<H265SeqParameterSet> sps;
};

#endif // H265PACKETIZER_H
