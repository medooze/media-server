#ifndef VP9_H
#define VP9_H
#include "config.h"
#include "log.h"

class VP9CodecConfig
{
public:
	DWORD Serialize(BYTE* buffer,DWORD length) const
	{
		if (length<GetSize())
			return 0;
		
		int l = 0;
		
		buffer[l++] = profile;
		buffer[l++] = level;
		buffer[l++] = (bitDepth << 4) | (chromaSubsampling << 1) | (videoFullRangeFlag ? 1 : 0);
		buffer[l++] = colorPrimaries;
		buffer[l++] = transferCharacteristics;
		buffer[l++] = matrixCoefficients;
		//add initialization data length
		set2(buffer,l,initializationData.size());
		//Inc leght
		l += 2;
		//Copy it
		memcpy(buffer+l,initializationData.data(),initializationData.size());
		//Inc length
		l += initializationData.size();
		//Done
		return l;
	}
	
	DWORD GetSize() const
	{
		return 8 + initializationData.size();
	}
private:
	uint8_t profile			= 0;
	uint8_t level			= 10;
	uint8_t bitDepth		= 8;
	uint8_t chromaSubsampling	= 0; //CHROMA_420
	bool videoFullRangeFlag		= false;
	uint8_t colorPrimaries		= 2; //Unspecified
	uint8_t transferCharacteristics	= 2; //Unspecified
	uint8_t matrixCoefficients	= 2; //Unspecified
	std::vector<uint8_t> initializationData;
};

#endif /* VP9_H */

