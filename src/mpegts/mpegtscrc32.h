#ifndef MPEGTSCRC32_H
#define MPEGTSCRC32_H

#include <stdint.h>
#include <stddef.h>

namespace mpegts {

extern uint32_t Crc32(const uint8_t *data, size_t len);

}

#endif
