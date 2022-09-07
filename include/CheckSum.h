#ifndef CHECKSUM_H_
#define CHECKSUM_H_

#include <stdint.h>
#include <stddef.h>

class CheckSum
{
public:
	/** 
	 * IMPORTANT: only the final call to Calculate (before Finalize) can have
	 * an odd size, otherwise the result will be incorrect (the odd byte should
	 * be buffered to be combined with the next one as a word)
	 */
	inline void Calculate(const void* __restrict__ data, size_t size)
	{
		for (size_t i = 0; i < (size & ~1); i = i + 2)
		{
			uint16_t word;
			memcpy(&word, ((const char*)data) + i, sizeof word);
			checksum += word;
		}
		if (size % 2 != 0)
			checksum += ((uint8_t*)data)[size - 1];
	}
	
	inline uint16_t Finalize()
	{
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
		return ~checksum;
	}

private:
	uint32_t checksum = 0;
};

#endif //CHECKSUM_H_
