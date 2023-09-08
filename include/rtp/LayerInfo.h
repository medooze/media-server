#ifndef LAYERINFO_H
#define LAYERINFO_H

#include "config.h"

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

