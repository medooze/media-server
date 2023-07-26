#ifndef OBU_H
#define OBU_H

#include "bitstream.h"

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
	
	uint8_t temporalId = 0;
	uint8_t spatialId = 0;
		
	DWORD Serialize(BYTE* buffer,DWORD bufferLength) const
	{
		if (bufferLength < 1) return 0;
		
		BitWritter writter(buffer, bufferLength);
		writter.Put(1, 0);
		writter.Put(4, type);
		writter.Put(1, extensionFlag);
		writter.Put(1, hasSizeField);
		writter.Put(1, 0);
		
		if (extensionFlag && writter.Left() > 0)
		{
			writter.Put(3, temporalId);
			writter.Put(2, spatialId);
		}
		
		auto written = bufferLength - writter.Left();
		return written;
	}
	
	size_t GetSize() const
	{
		size_t sz = 1;
		if (extensionFlag) sz++;
		
		return sz;
	}
	
	bool Parse(const BYTE* data, DWORD size)
	{
		if (size < 1) return false;
		
		BitReader reader(data, size);
		
		auto forbiddenBit = reader.Get(1);
		if (forbiddenBit != 0) return false;
		
		type = reader.Get(4);
		extensionFlag = reader.Get(1);
		hasSizeField = reader.Get(1);		
		(void) reader.Get(1);
		
		size--;
		
		if (extensionFlag)
		{
			if (size < 1) return false;
			
			temporalId = reader.Get(3);
			spatialId = reader.Get(2);
		}
				
		return true;	
	}
};

struct ObuInfo
{
	size_t obuSize = 0;
	uint8_t obuType = 0;
	size_t headerSize = 0;
	const uint8_t* payload = nullptr;
	size_t payloadSize = 0;	
};

class ObuHelper
{
public:
	static std::optional<ObuInfo> GetObuInfo(const BYTE* data, DWORD size)
	{
		ObuHeader header;
		if (!header.Parse(data, size)) return std::nullopt;
		
		size_t headerSize = header.GetSize();
		
		size_t payloadSize = 0;		
		BufferReader reader(data + headerSize, size - headerSize);
		if (header.hasSizeField)
		{
			payloadSize = reader.DecodeLev128();			
		}
		
		auto* payload = reader.GetData(payloadSize);
		ObuInfo info {(payload - data) + payloadSize, header.type, headerSize, payload, payloadSize};
		
		return info;
	}
};

#endif
