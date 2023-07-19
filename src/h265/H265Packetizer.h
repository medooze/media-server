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

	/** this is not private because MPEGTSHEVCStream needs to access it
	    to know when to start allowing frames through */
	HEVCDescriptor config;

	std::unique_ptr<VideoFrame> ProcessAU(BufferReader reader) override;

protected:
	void OnNal(VideoFrame& frame, BufferReader& nal) override;

	void EmitNal(VideoFrame& frame, BufferReader nal)
	{
		auto naluHeader 		= nal.Peek2();
		BYTE nalUnitType		= (naluHeader >> 9) & 0b111111;
		BYTE nuh_layer_id		= (naluHeader >> 3) & 0b111111;
		BYTE nuh_temporal_id_plus1	= naluHeader & 0b111;

		const uint16_t nalHeaderFU = ((uint16_t)(HEVC_RTP_NALU_Type::UNSPEC49_FU) << 9)
							| ((uint16_t)(nuh_layer_id) << 3)
							| ((uint16_t)(nuh_temporal_id_plus1)); 
		std::string fuPrefix = {static_cast<char>((nalHeaderFU & 0xff00) >> 8), static_cast<char>(nalHeaderFU & 0xff), (char)nalUnitType};
		H26xPacketizer::EmitNal(frame, nal, fuPrefix, HEVCParams::RTP_NAL_HEADER_SIZE);
	}

	bool noPPSInFrame, noSPSInFrame, noVPSInFrame;
};

#endif // H265PACKETIZER_H
