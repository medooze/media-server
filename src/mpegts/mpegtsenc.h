#ifndef MPEGTSENC_H
#define MPEGTSENC_H


#include "Buffer.h"
#include "media.h"

#include <vector>
#include <map>
#include <stdint.h>

namespace mpegts
{

std::vector<Buffer> CreatePAT(uint16_t programNumber, uint16_t pmtPid, uint8_t continutyCounter);

std::vector<Buffer> CreatePMT(uint16_t pid, std::map<uint16_t, uint16_t> streamPidMap, uint8_t continutyCounter);

std::vector<Buffer> CreatePES(uint16_t pid, const uint8_t* media, size_t size, uint8_t& continutyCounter);

}

#endif