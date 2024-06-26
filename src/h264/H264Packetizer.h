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

	void AppendScteData(uint64_t ts, const Buffer& data, uint8_t repeatCount);
	
protected:
	void OnNal(VideoFrame& frame, BufferReader& nal, std::optional<bool>& frameEnd) override;
	void EmitNal(VideoFrame& frame, BufferReader nal);
	AVCDescriptor config;
	bool noPPSInFrame = true;
	bool noSPSInFrame = true;
	
	std::unique_ptr<H264SeqParameterSet> sps;
	
	Buffer scteMessage;
	uint64_t scteTimestamp;
	// To account for potential packet loss, we repeat the SEI containing 
	// scte tags for 20 frames to provide added resiliency.
	// Also, we assume that another scte message cannot arrive within is interval
	uint8_t scteFrameRepeatCount;
	uint8_t repeatFrameCounter;
	//Unregistered SEI message NAL
	std::vector<uint8_t> sei;
	uint16_t scteMessageId;
};

#endif // H264PACKETIZER_H
