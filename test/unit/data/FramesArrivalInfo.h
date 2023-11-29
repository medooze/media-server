#ifndef FRAMESARRIVALINFO_H
#define FRAMESARRIVALINFO_H

#include "media.h"

#include <vector>
#include <tuple>

namespace TestData
{
	
extern std::vector<std::tuple<MediaFrame::Type, uint64_t, uint64_t, uint32_t>> FramesArrivalInfo;

extern std::vector<std::tuple<MediaFrame::Type, uint64_t, uint64_t, uint32_t>> FramesArrivalInfoLargeAVDesync;
	
}


#endif