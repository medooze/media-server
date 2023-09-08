#ifndef VIDEOLAYERSALLOCATION_H_
#define VIDEOLAYERSALLOCATION_H_

#include "config.h"
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "bitstream.h"

/*
* https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-layers-allocation00
*
*				   +-+-+-+-+-+-+-+-+
*				   |RID| NS| sl_bm |
*				   +-+-+-+-+-+-+-+-+
*	 Spatial layer bitmask     |sl0_bm |sl1_bm |
*	   up to 2 bytes           |---------------|
*	   when sl_bm == 0         |sl2_bm |sl3_bm |
*				   +-+-+-+-+-+-+-+-+
*	 Number of temporal layers |#tl|#tl|#tl|#tl|
*	 per spatial layer         |   |   |   |   |
*				   +-+-+-+-+-+-+-+-+
*	  Target bitrate in kpbs   |               |
*	   per temporal layer      :      ...      :
*	    leb128 encoded         |               |
*				   +-+-+-+-+-+-+-+-+
*	 Resolution and framerate  |               |
*	 5 bytes per spatial layer + width-1 for   +
*	      (optional)           | RID=0, sid=0  |
*				   +---------------+
*				   |               |
*				   + height-1 for  +
*				   | RID=0, sid=0  |
*				   +---------------+
*				   | max framerate |
*				   +-+-+-+-+-+-+-+-+
*				   :      ...      :
*				   +-+-+-+-+-+-+-+-+
*/
struct VideoLayersAllocation
{
	static constexpr int MaxStreams = 4;
	static constexpr int MaxSpatialIds = 4;
	static constexpr int MaxTemporalIds = 4;

	struct SpatialLayer
	{
		SpatialLayer(int streamIdx, int spatialId, int numTemporalLayers) :
			streamIdx(streamIdx),
			spatialId(spatialId),
			targetBitratePerTemporalLayer(numTemporalLayers, 0)
		{
		}

		SpatialLayer(int streamIdx,
				int spatialId,
				const std::vector<uint16_t>& targetBitratePerTemporalLayer,
				std::optional<uint16_t> width = std::nullopt,
				std::optional<uint16_t> height = std::nullopt,
				std::optional<uint8_t> fps = std::nullopt
			) :
			streamIdx(streamIdx),
			spatialId(spatialId),
			targetBitratePerTemporalLayer(targetBitratePerTemporalLayer),
			width(width),
			height(height),
			fps(fps)
		{
		}

		int streamIdx = 0;
		// Index of the spatial layer per `rtp_stream_index`.
		int spatialId = 0;

		// Target bitrate per decode target in kbps.
		std::vector<uint16_t> targetBitratePerTemporalLayer;
		std::optional<uint16_t> width;
		std::optional<uint16_t> height;
		// Max frame rate used in any temporal layer of this spatial layer.
		std::optional<uint8_t> fps;

		bool operator== (const SpatialLayer& other) const
		{
			return streamIdx == other.streamIdx
				&& spatialId == other.spatialId
				&& targetBitratePerTemporalLayer == other.targetBitratePerTemporalLayer
				&& width == other.width
				&& height == other.height
				&& fps == other.fps;
		}

		void Dump() const
		{
			Debug("[SpatialLayer streamIdx=%d spatialId=%d]\n", streamIdx, spatialId);
			for (const auto& bitrate : targetBitratePerTemporalLayer)
				Debug("\t\t[/ActiveSpatialLayers bitrate=%d/]\n", bitrate);
			if (width && height && fps)
				Debug("[Optionals width=%d height=%d fps=%d/]\n", *width, *height, *fps);
			Debug("[/SpatialLayer]\n");
		}

	};

	// Index of the rtp stream this allocation is sent on. Used for mapping a SpatialLayer to a rtp stream.
	uint8_t streamIdx = 0;
	uint8_t numRtpStreams = 0;
	std::vector<SpatialLayer> activeSpatialLayers;


	std::array<std::array<bool, MaxSpatialIds>, MaxStreams> GetSpatialLayersMask() const
	{
		std::array<std::array<bool, MaxSpatialIds>, MaxStreams> spatialLayesrMask{};

		for (const auto& activeLayer: activeSpatialLayers)
			if (activeLayer.streamIdx < MaxStreams && activeLayer.spatialId < MaxSpatialIds)
				spatialLayesrMask[activeLayer.streamIdx][activeLayer.spatialId] = true;

		return spatialLayesrMask;
	}

	bool operator== (const VideoLayersAllocation& other) const
	{
		return streamIdx == other.streamIdx
			&& numRtpStreams == other.numRtpStreams
			&& activeSpatialLayers == other.activeSpatialLayers;
	}

	void Dump() const
	{
		Debug("[VideoLayersAllocation streamIdx=%d numRtpStreams=%d]\n", streamIdx, numRtpStreams);
		auto spatialLayesrMask = GetSpatialLayersMask();
		for (int i = 0; i < MaxSpatialIds; ++i)
			Debug("\t[SpatialLayerMask  layer=%d mask=%d%d%d%d]\n", i, spatialLayesrMask[i][0], spatialLayesrMask[i][1], spatialLayesrMask[i][2], spatialLayesrMask[i][3]);
		for (const auto& layer : activeSpatialLayers)
			layer.Dump();
		Debug("[/VideoLayersAllocation\n]\n");
	}
};

#endif //VIDEOLAYERSALLOCATION_H_