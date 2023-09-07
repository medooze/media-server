#ifndef LAYERINFO_H
#define LAYERINFO_H

#include "config.h"
#include <optional>

struct LayerInfo
{
	LayerInfo() = default;
	LayerInfo(BYTE temporalLayerId, BYTE spatialLayerId) :
		temporalLayerId(temporalLayerId),
		spatialLayerId(spatialLayerId)
	{	
	}
	static BYTE MaxLayerId;
	static LayerInfo NoLayer;
	
	BYTE temporalLayerId = MaxLayerId;
	BYTE spatialLayerId  = MaxLayerId;

	// From VideoLayerAllocation info
	bool active = true;
	std::optional<uint16_t> targetBitrate;
	std::optional<uint16_t> targetWidth;
	std::optional<uint16_t> targetHeight;
	std::optional<uint8_t>  targetFps;
	
	bool IsValid() const { return spatialLayerId!=MaxLayerId || temporalLayerId != MaxLayerId;	}
	WORD GetId()   const { return ((WORD)spatialLayerId)<<8  | temporalLayerId;			}
	
	friend bool operator==(const LayerInfo& lhs, const LayerInfo& rhs)
	{
		return  lhs.temporalLayerId == rhs.temporalLayerId &&
			lhs.spatialLayerId == rhs.spatialLayerId;
	}
	friend bool operator!=(const LayerInfo& lhs, const LayerInfo& rhs)
	{
		return !(lhs == rhs);
	}
};

#endif /* LAYERINFO_H */

