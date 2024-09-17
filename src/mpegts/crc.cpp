#include "crc.h"

uint32_t crc32(const uint8_t *data, int len)
{
    int i;
    uint32_t crc = 0xffffffff;

    for (i = 0; i<len; i++)
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];

    return crc;
}
