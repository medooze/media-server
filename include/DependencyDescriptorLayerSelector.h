#ifndef DEPENDENCYDESCRIPTORLAYERSELECTOR_H
#define DEPENDENCYDESCRIPTORLAYERSELECTOR_H

#include "codecs.h"
#include "WrapExtender.h"
#include "VideoLayerSelector.h"


class DependencyDescriptorLayerSelector : 
	public VideoLayerSelector
{
public:
	DependencyDescriptorLayerSelector(VideoCodec::Type codec);
	virtual ~DependencyDescriptorLayerSelector() = default;
	void SelectTemporalLayer(BYTE id)		override;
	void SelectSpatialLayer(BYTE id)		override;
	
	bool Select(const RTPPacket::shared& packet,bool &mark)	override;
	
	BYTE GetTemporalLayer()		const override { return temporalLayerId;	}
	BYTE GetSpatialLayer()		const override { return spatialLayerId;		}
	VideoCodec::Type GetCodec()	const override { return codec;			}
	bool IsWaitingForIntra()	const override { return false;			}
	
	static LayerInfo GetLayerIds(const RTPPacket::shared& packet);
private:
	WrapExtender<uint16_t,uint32_t> frameNumberExtender;
	uint32_t currentFrameNumber = std::numeric_limits<uint32_t>::max();
	TemplateDependencyStructure currentTemplateDependencyStructure;
	std::vector<uint32_t> forwardedFrames;
	DWORD currentDecodeTarget = 0;
	DWORD nextDecodeTarget = 0;
	
	VideoCodec::Type codec;
	bool waitingForIntra;
	BYTE temporalLayerId;
	BYTE spatialLayerId;
	BYTE nextTemporalLayerId;
	BYTE nextSpatialLayerId;
};

#endif /* DEPENDENCYDESCRIPTORLAYERSELECTOR_H */

