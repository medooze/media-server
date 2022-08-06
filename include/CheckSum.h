#ifndef CHECKSUM_H_
#define CHECKSUM_H_

#include <stdint.h>
#include <stddef.h>

class CheckSum
{
public:
	inline void Calculate(const char* data, size_t size)
	{
		for (size_t i = 0; i < size *2; i++)
		{
			uint16_t word;
			memcpy(&word, ((char*)data) + i * 2, sizeof word);
			checksum += word;
		}
		if (size % 2 != 0)
			checksum += uint16_t(data[size - 1]);
	}
	inline uint32_t Finalize()
	{
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
		checksum = ~checksum;
		if (!checksum) checksum = 0xFFFF;
		return checksum;
	}
private:
	uint32_t checksum = 0;
};

#endif //CHECKSUM_H_
