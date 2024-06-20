#ifndef H264PACKETIZER_H
#define H264PACKETIZER_H

#include "H26xPacketizer.h"
#include "avcdescriptor.h"

#include <queue>

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

	void PushScte(Buffer data);
	std::optional<Buffer> PopScte();
	void PushScteTimestamp(uint64_t data);
	std::optional<uint64_t> PopScteTimestamp();
	
protected:
	void OnNal(VideoFrame& frame, BufferReader& nal, std::optional<bool>& frameEnd) override;
	void EmitNal(VideoFrame& frame, BufferReader nal);
	AVCDescriptor config;
	bool noPPSInFrame = true;
	bool noSPSInFrame = true;
	
	std::unique_ptr<H264SeqParameterSet> sps;
	
	std::queue<Buffer> scteMessages;
	std::queue<uint64_t> scteTimestamps;
	u_int8_t scteFrameRepeatCount;
};

#endif // H264PACKETIZER_H
