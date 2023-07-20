#ifndef CODECCONFIGURATIONRECORD_H
#define CODECCONFIGURATIONRECORD_H

#include "MediaUnitPacketizer.h"
#include <functional>

class CodecConfigurationRecord
{
public:
	
	virtual bool Parse(const uint8_t* input, DWORD t) = 0;
	virtual DWORD Serialize(uint8_t* output, DWORD t) const = 0;
	
	virtual bool GetResolution(unsigned& width, unsigned& height) const = 0;
	virtual DWORD GetSize() const = 0;
	virtual BYTE GetNALUnitLengthSizeMinus1() const = 0;
	
	virtual void Map(const std::function<void (const uint8_t*, size_t)>& func) = 0;
};


#endif