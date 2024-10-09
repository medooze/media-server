#ifndef VIDEOLAYERSALLOCATION_H_
#define VIDEOLAYERSALLOCATION_H_

#include "config.h"
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "BufferReader.h"
#include "BufferWritter.h"
#include "bitstream/BitReader.h"

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

	static std::optional<VideoLayersAllocation> Parse(BufferReader& reader)
	{

		std::optional<VideoLayersAllocation> videoLayersAllocation = std::make_optional<VideoLayersAllocation>();

		try {
			//If it is single layer with no info
			if (reader.GetLeft() == 1 && reader.Peek1() == 0)
			{
				//Everything went well
				videoLayersAllocation->streamIdx = 0;
				//Done
				return videoLayersAllocation;
			}

			// Header byte.
			BitReader bitreader(reader);

			//Get rid, num streams
			videoLayersAllocation->streamIdx = bitreader.Get(2);
			videoLayersAllocation->numRtpStreams = 1 + bitreader.Get(2);
			size_t numActiveLayers = 0;

			std::array<std::array<bool, VideoLayersAllocation::MaxSpatialIds>, VideoLayersAllocation::MaxStreams> spatialLayesrMask{};

			//Read "master" layer mask
			for (auto j = 0; j < VideoLayersAllocation::MaxSpatialIds; ++j)
				//Get number of active layers and update mask
				numActiveLayers += spatialLayesrMask[0][j] = bitreader.Get(1);

			//if mask is not empty
			if (numActiveLayers)
			{
				//Fill all the other stream with the same mask
				for (int i = 1; i < videoLayersAllocation->numRtpStreams && i < VideoLayersAllocation::MaxStreams; ++i)
					//Get number of active layers and update mask for stream
					spatialLayesrMask[i] = spatialLayesrMask[0];
				//Set total layers for all streams
				numActiveLayers = numActiveLayers * videoLayersAllocation->numRtpStreams;
			}
			// Spatial layer bitmasks when they are different for different RTP streams.
			else
			{
				//For each stream
				for (int i = 0; i < videoLayersAllocation->numRtpStreams; ++i)
					//for each layer
					for (auto j = 0; j < VideoLayersAllocation::MaxSpatialIds; ++j)
						//Get number of active layers and update mask for stream
						numActiveLayers += spatialLayesrMask[i][j] = bitreader.Get(1);
			}

			//Zero pad
			bitreader.Align();

			//Reserve mem for active layers
			videoLayersAllocation->activeSpatialLayers.reserve(numActiveLayers);

			// Read number of temporal layers for each stream
			for (int streamIdx = 0; streamIdx < videoLayersAllocation->numRtpStreams; ++streamIdx)
			{
				//For each spatial layer
				for (int spatialId = 0; spatialId < VideoLayersAllocation::MaxSpatialIds; ++spatialId)
				{
					//If the layer is not active
					if (spatialLayesrMask[streamIdx][spatialId] == 0)
						//No temporal info available for it
						continue;

					//Create new active layer
					videoLayersAllocation->activeSpatialLayers.emplace_back(streamIdx, spatialId, bitreader.Get(2));
				}
			}

			//Done reading
			bitreader.Flush();
			
			//Target bitrates for each active spatial layer
			for (auto& layer : videoLayersAllocation->activeSpatialLayers)
			{
				//For each temporal layer of the spatial layer
				for (auto& rate : layer.targetBitratePerTemporalLayer)
				{
					if (reader.GetLeft() == 0)
						//Error
						throw std::range_error("no more bytes to read from byte buffer");

					//In kbps
					rate = reader.DecodeLev128();
				}
			}

			//Check we have enough size left
			if (reader.GetLeft() >= 5 * numActiveLayers)
			{
				//For each active layer
				for (auto& layer : videoLayersAllocation->activeSpatialLayers)
				{
					//Get all dimenensions and fps 
					layer.width = 1 + reader.Get2();
					layer.height = 1 + reader.Get2();
					layer.fps = reader.Get1();
				}
			}
		}
		catch (std::exception& e)
		{
			UltraDebug("-VideoLayersAllocation::Parse() Error parsing %s\n", e.what());
			return std::nullopt;
		}

		//Done
		return videoLayersAllocation;
	}

	DWORD Serialize(BufferWritter& writer) const
	{
		try{
			if (!activeSpatialLayers.size())
			{
				//Nothing active
				writer.Set1(0);
			}
			else 
			{
				auto spatialLayesrMask = GetSpatialLayersMask();

				bool allEqual = true;
				//For all except the first one
				for (size_t i = 1; i < spatialLayesrMask.size() && i < numRtpStreams && allEqual; ++i)
					//Check if 
					allEqual = std::equal(
						spatialLayesrMask[0].begin(),
						spatialLayesrMask[0].end(),
						spatialLayesrMask[i].begin()
					);

				//Extension header
				uint8_t header = 0;
				BitWriter headerBitWriter(&header, 1);

				//Set rid and number of streams
				headerBitWriter.Put(2, streamIdx);
				headerBitWriter.Put(2, numRtpStreams - 1);

				//If using master
				if (allEqual)
					//Get active layer mask
					for (const auto& active : spatialLayesrMask[0])
						//Write in header
						headerBitWriter.Put(1, active);

				//Flush header
				headerBitWriter.Flush();

				//Write header to extension
				writer.Set1(header);

				//If each layer is different
				if (!allEqual)
				{
					//Active spatial layer mask for each stream
					std::array<uint8_t, VideoLayersAllocation::MaxStreams / 2> mask = {};
					BitWriter maskBitWriter(mask.data(), mask.size());

					//For each stream
					for (auto i = 0; i < numRtpStreams && i < VideoLayersAllocation::MaxStreams; ++i)
						//for each layer in stream
						for (const auto& active : spatialLayesrMask[i])
							//Write in mask
							maskBitWriter.Put(1, active);
					//Flush mask
					int len = maskBitWriter.Flush();

					//Set mask in extension
					writer.SetN(mask, len);
				}

				//Temporal layers for each spatial layer
				std::array<uint8_t, VideoLayersAllocation::MaxSpatialIds / 4> numTemporalLayers = {};
				BitWriter numTemporalLayersBitWriter(numTemporalLayers.data(), numTemporalLayers.size());

				//For each active spatial layer
				for (const auto& spatialLayer : activeSpatialLayers)
				{
					//Set the number of layers
					numTemporalLayersBitWriter.Put(2, spatialLayer.targetBitratePerTemporalLayer.size());
				}

				//Flush temporal layers number
				int len = numTemporalLayersBitWriter.Flush();

				//Set  temporal layers number in extension
				writer.SetN(numTemporalLayers, len);

				bool optionalResolutionAndFrameRate = true;

				//Target bitrates.
				for (const auto& spatialLayer : activeSpatialLayers)
				{
					//Get rata for each temporal layer
					for (const auto& bitrate : spatialLayer.targetBitratePerTemporalLayer)
						//Write it
						writer.EncodeLeb128(bitrate);
					//Check if we have all the optional info
					optionalResolutionAndFrameRate &= spatialLayer.width.has_value() && spatialLayer.width.value() > 0
						&& spatialLayer.height.has_value() && spatialLayer.height.value() > 0
						&& spatialLayer.fps.has_value();
				}

				//If all active layers have the optional info
				if (optionalResolutionAndFrameRate)
				{
					for (const auto& spatialLayer : activeSpatialLayers)
					{
						//Write them
						writer.Set2(spatialLayer.width.value() - 1);
						writer.Set2(spatialLayer.height.value() - 1);
						writer.Set1(spatialLayer.fps.value());
					}
				}
			}
		}
		catch (std::exception& e) 
		{
			return false;
		}

		return true;
	}

};

#endif //VIDEOLAYERSALLOCATION_H_