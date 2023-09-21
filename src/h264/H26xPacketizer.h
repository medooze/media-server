#ifndef H26XPACKETIZER_H
#define H26XPACKETIZER_H

#include "video.h"
#include "rtp/RTPPacketizer.h"

/**
 * base class for H.264 and H.265 packetizers containing logic common to both:
 *  - slicing incoming NALUs
 *  - NALU fragmentation in RTP
 *  - frame size caching
 */
class H26xPacketizer : public RTPPacketizer
{
public:
	// the packetizer only supports 4-byte length as output
	static constexpr size_t OUT_NALU_LENGTH_SIZE = 4;

	H26xPacketizer(VideoCodec::Type codec);
	virtual std::unique_ptr<MediaFrame> ProcessAU(BufferReader& payload);

	void ResetFrame();
	
protected:
	/** called by ProcessAU() for every NALU found in the AU */
	virtual void OnNal(VideoFrame& frame, BufferReader& nal, std::optional<bool>& frameEnd) = 0;

	/**
	 * @brief add a NALU to the frame, fragmenting it in RTP if necessary
	 * 
	 * @param fuPrefix the FU prefix, including the FU header byte, with the S and E bits not filled in
	 */
	void EmitNal(VideoFrame& frame, BufferReader nal, std::string& fuPrefix, int naluHeaderSize);

private:

	std::unique_ptr<VideoFrame> currentFrame;

	// cached frame size
	uint32_t width = 0;
	uint32_t height = 0;
};

#endif // H26XPACKETIZER_H
