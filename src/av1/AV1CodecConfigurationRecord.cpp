#include "AV1CodecConfigurationRecord.h"
#include <cassert>


bool AV1CodecConfigurationRecord::Parse(const uint8_t* input, DWORD t)
{
	Debug("Parsing AV1CodecConfigurationRecord\n");
	
	// @todo Implement this function
	
	return t;
}

DWORD AV1CodecConfigurationRecord::Serialize(uint8_t* output, DWORD t) const
{
	// @todo Implement this function
	
	return t;
}

bool AV1CodecConfigurationRecord::GetResolution(unsigned& width, unsigned& height) const
{
	// @todo Implement this function
	
	return true;
}

DWORD AV1CodecConfigurationRecord::GetSize() const
{
	// @todo Implement this function
	
	return 0;
}

BYTE AV1CodecConfigurationRecord::GetNALUnitLengthSizeMinus1() const
{
	// @todo Implement this function
	return 0;
}

void AV1CodecConfigurationRecord::Map(const std::function<void (const uint8_t*, size_t)>& func)
{
	// @todo Implement this function
}