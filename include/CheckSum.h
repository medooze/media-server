#ifndef CHECKSUM_H_
#define CHECKSUM_H_

#include <stdint.h>
#include <stddef.h>

class CheckSum
{
public:
	inline void Calculate(const char* data, size_t size)
	{
		for (size_t i = 0; i < (size & ~1); i = i + 2)
		{
			uint16_t word;
			memcpy(&word, data + i, sizeof word);
			checksum += word;
		}
		if (size % 2 != 0)
			checksum += ((uint8_t*)data)[size - 1];
	}
	
	inline uint16_t Finalize()
	{
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
		uint32_t res = ~checksum;
		if (!res) res = 0xFFFF;
		return res;
	}

private:
	uint32_t checksum = 0;
};

#endif //CHECKSUM_H_
