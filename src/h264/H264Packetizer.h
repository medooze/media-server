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

	/** this is not private because MPEGTSAVCStream needs to access it
	    to know when to start allowing frames through */
	AVCDescriptor config;

	std::unique_ptr<VideoFrame> ProcessAU(BufferReader reader) override;

protected:
	void OnNal(VideoFrame& frame, BufferReader& nal) override;

	void EmitNal(VideoFrame& frame, BufferReader nal)
	{
		uint8_t naluHeader 	= nal.Peek1();
		uint8_t nri		= naluHeader & 0b0'11'00000;
		uint8_t nalUnitType	= naluHeader & 0b0'00'11111;

		std::string fuPrefix = { (char)(nri | 28u), (char)nalUnitType };
		H26xPacketizer::EmitNal(frame, nal, fuPrefix, 1);
	}

	bool noPPSInFrame, noSPSInFrame;
};

#endif // H264PACKETIZER_H
