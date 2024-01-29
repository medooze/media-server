#ifndef VP9_H
#define VP9_H
#include "config.h"
#include "log.h"
#include "bitstream.h"

#include <optional>

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

class VP9FrameHeader
{
public:
	enum FrameType
	{
		KEY_FRAME = 0,
		NON_KEY_FRAME = 1
	};
	
	enum ColorSpace
	{
		CS_UNKNOWN = 0,
		CS_BT_601 = 1,
		CS_BT_709 = 2,
		CS_SMPTE_170 = 3,
		CS_SMPTE_240 = 4,
		CS_BT_2020 = 5,
		CS_RESERVED = 6,
		CS_RGB = 7
	};

	bool Parse(const uint8_t *data, uint8_t size)
	{
		BitReader reader(data, size);
		
		auto frame_marker = reader.Get(2);
		if (frame_marker != 2) return false;
		
		auto profile_low_bit = reader.Get(1);
		auto profile_high_bit = reader.Get(1);
		
		profile = profile_low_bit + (profile_high_bit << 1);
		if (profile == 3) reader.Get(1);
		
		show_existing_frame = reader.Get(1);
		if (show_existing_frame)
		{
			frame_to_show_map_idx = reader.Get(3);
		}
		else
		{
			frame_type = FrameType(reader.Get(1));
			show_frame = reader.Get(1);
			error_resilient_mode = reader.Get(1);
			
			if (frame_type == FrameType::KEY_FRAME)
			{
				ParseFrameSyncCode(reader);
				ParseColorConfig(reader);
				ParseFrameSize(reader);
			}
		}
		
		return !reader.Error();
	}
	
	inline const std::optional<FrameType>& GetFrameType() const
	{
		return frame_type;
	}
	
	inline const std::optional<uint16_t>& GetFrameWidthMinus1() const
	{
		return frame_width_minus_1;
	}
	
	inline const std::optional<uint16_t>& GetFrameHeightMinus1() const
	{
		return frame_height_minus_1;
	}
	
private:
	void ParseFrameSyncCode(BitReader& reader)
	{
		reader.Get(8);
		reader.Get(8);
		reader.Get(8);
	}
	
	void ParseColorConfig(BitReader& reader)
	{
		if (profile >= 2)
		{
			ten_or_twelve_bit = reader.Get(1);
		}
		color_space = ColorSpace(reader.Get(3));
		if (color_space.value() != ColorSpace::CS_RGB)
		{
			color_range = reader.Get(1);
			if (profile == 1 || profile == 3)
			{
				subsampling_x = reader.Get(1);
				subsampling_y = reader.Get(1);
				reader.Get(1); // reserved_zero
			}
			else
			{
				subsampling_x = 1;
				subsampling_y = 1;
			}
		}
		else
		{
			color_range = 1;
			if (profile == 1 || profile == 3)
			{
				subsampling_x = 0;
				subsampling_y = 0;
				reader.Get(1);
			}
		}
	}
	
	void ParseFrameSize(BitReader& reader)
	{
		frame_width_minus_1 = reader.Get(16);
		frame_height_minus_1 = reader.Get(16);
	}

	uint8_t show_existing_frame = 0;
	std::optional<uint8_t> frame_to_show_map_idx;
	std::optional<FrameType> frame_type;
	std::optional<uint8_t> show_frame;
	std::optional<uint8_t> error_resilient_mode;
	
	std::optional<uint8_t> ten_or_twelve_bit;
	std::optional<ColorSpace> color_space;
	std::optional<uint8_t> color_range;
	std::optional<uint8_t> subsampling_x;
	std::optional<uint8_t> subsampling_y;
	
	std::optional<uint16_t> frame_width_minus_1;
	std::optional<uint16_t> frame_height_minus_1;
	
	uint8_t profile = 0;
};

#endif /* VP9_H */

