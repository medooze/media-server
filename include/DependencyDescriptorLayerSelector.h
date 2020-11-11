#ifndef DEPENDENCYDESCRIPTORLAYERSELECTOR_H
#define DEPENDENCYDESCRIPTORLAYERSELECTOR_H

#include "codecs.h"
#include "WrapExtender.h"
#include "VideoLayerSelector.h"
#include "BitHistory.h"


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
	bool IsWaitingForIntra()	const override { return waitingForIntra;	}
	
	std::optional<std::vector<bool>> GetForwardedDecodeTargets() const { return forwardedDecodeTargets;	}
	
	static std::vector<LayerInfo> GetLayerIds(const RTPPacket::shared& packet);
private:
	WrapExtender<uint16_t,uint64_t> frameNumberExtender;
	uint64_t currentFrameNumber = std::numeric_limits<uint64_t>::max();
	BitHistory<256> forwardedFrames;
	std::optional<std::vector<bool>> forwardedDecodeTargets;
	
	VideoCodec::Type codec;
	BYTE temporalLayerId = LayerInfo::MaxLayerId;
	BYTE spatialLayerId  = LayerInfo::MaxLayerId;
	bool waitingForIntra = true;
};

#endif /* DEPENDENCYDESCRIPTORLAYERSELECTOR_H */

