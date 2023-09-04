#ifndef AV1CODECCONFIGURATIONRECORD_H
#define AV1CODECCONFIGURATIONRECORD_H

#include "AV1.h"
#include <optional>

class AV1CodecConfigurationRecord
{
public:
	struct Fields
	{
		uint8_t version 		: 7;
		uint8_t marker 			: 1;
			
		uint8_t seq_level_idx_0 	: 5 ;
		uint8_t seq_profile 		: 3;
		
		uint8_t chroma_sample_position 	: 2;
		uint8_t chroma_subsampling_y 	: 1;
		uint8_t chroma_subsampling_x 	: 1;		
		uint8_t monochrome 		: 1;
		uint8_t twelve_bit 		: 1;
		uint8_t high_bitdepth 		: 1;
		uint8_t seq_tier_0		: 1 ;
		
		uint8_t initial_presentation_delay_minus_one 	: 4;
		uint8_t initial_presentation_delay_present 	: 1 ;
		uint8_t reserved 				: 3 ;
	};

	DWORD Serialize(BYTE* buffer,DWORD bufferLength) const;
	bool Parse(const BYTE* data,DWORD size);
	
	size_t GetSize() const;
		
	Fields fields;
	
	std::optional<std::vector<uint8_t>> sequenceHeader;
};


#endif

