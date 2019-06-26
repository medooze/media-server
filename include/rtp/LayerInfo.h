#ifndef LAYERINFO_H
#define LAYERINFO_H

#include "config.h"

struct LayerInfo
{
	static BYTE MaxLayerId; 
	BYTE temporalLayerId = MaxLayerId;
	BYTE spatialLayerId  = MaxLayerId;
	
	bool IsValid() const { return spatialLayerId!=MaxLayerId || temporalLayerId != MaxLayerId;	}
	WORD GetId()   const { return ((WORD)spatialLayerId)<<8  | temporalLayerId;			}
};

#endif /* LAYERINFO_H */

