#ifndef AV1LAYERSELECTOR_H
#define AV1LAYERSELECTOR_H

#include "DependencyDescriptorLayerSelector.h"


class AV1LayerSelector : public DependencyDescriptorLayerSelector
{
public:
	AV1LayerSelector();
	
	static std::vector<LayerInfo> GetLayerIds(const RTPPacket::shared& packet);
};



#endif