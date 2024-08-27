#ifndef OBU_H
#define OBU_H

#include "log.h"
#include "bitstream.h"
#include "BufferReader.h"
#include "BufferWritter.h"
#include "rtp/LayerInfo.h"
#include <optional>

enum ObuType
{
	ObuSequenceHeader = 1,
	ObuTemporalDelimiter = 2,
	ObuFrameHeader = 3,
	ObuTileGroup = 4,
	ObuMetadata = 5,
	ObuFrame = 6,
	ObuRedundantFrameHeader = 7,
	ObuTileList = 8,
	ObuPadding = 15
};

class ObuHeader
{
public: 
	struct Extension
	{
		LayerInfo layerInfo;
	};
public:
	uint8_t type = 0;
	std::optional<Extension> extension;
	std::optional<uint64_t> length;
	
	DWORD Serialize(BufferWritter& writter) const;
	inline DWORD Serialize(BYTE* buffer, DWORD bufferLength) const
	{
		BufferWritter writter(buffer, bufferLength);
		return Serialize(writter);
	}
	
	size_t GetSize() const;
	
	bool Parse(BufferReader& reader);
	inline bool Parse(const BYTE* data, DWORD size)
	{
		BufferReader reader(data, size);
		return Parse(reader);
	}
	
	inline void Dump()
	{
		Debug("[ObuHeader \n");
		Debug("\ttype=%u\n", type);
		if (extension)
			Debug("\textension.layerInfo=[%u,%u]\n", extension->layerInfo.temporalLayerId, extension->layerInfo.spatialLayerId);
		if (length)
			Debug("\textension.layerInfo=[%u,%u]\n", extension->layerInfo.temporalLayerId, extension->layerInfo.spatialLayerId);
		if (length)
			Debug("\tlength=%u\n", length.value());
		Debug("/]\n");
	}
};

#endif
