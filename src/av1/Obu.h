#ifndef OBU_H
#define OBU_H

#include "bitstream.h"

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
	uint8_t type = 0;
	bool extensionFlag = 0;
	bool hasSizeField = 0;
	
	// Extention fields
	uint8_t temporalId = 0;
	uint8_t spatialId = 0;
		
	DWORD Serialize(BYTE* buffer,DWORD bufferLength) const;
	
	size_t GetSize() const;
	
	bool Parse(const BYTE* data, DWORD size);
};

struct ObuInfo
{
	size_t obuSize = 0;
	uint8_t obuType = 0;
	size_t headerSize = 0;
	const uint8_t* payload = nullptr;
	size_t payloadSize = 0;	
};

std::optional<ObuInfo> GetObuInfo(const BYTE* data, DWORD size);

#endif
