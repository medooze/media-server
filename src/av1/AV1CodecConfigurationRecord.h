#ifndef AV1CODECCONFIGURATIONRECORD_H
#define AV1CODECCONFIGURATIONRECORD_H

#include "CodecConfigurationRecord.h"

class AV1CodecConfigurationRecord : public CodecConfigurationRecord
{
public:

	virtual bool Parse(const uint8_t* input, DWORD t) override;
	virtual DWORD Serialize(uint8_t* output, DWORD t) const override;
	
	virtual bool GetResolution(unsigned& width, unsigned& height) const override;
	virtual DWORD GetSize() const override;
	virtual BYTE GetNALUnitLengthSizeMinus1() const override;
	
	virtual void Map(const std::function<void (const uint8_t*, size_t)>& func) override;
	
};


#endif