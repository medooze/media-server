#ifndef H265LAYERSELECTOR_H
#define H265LAYERSELECTOR_H

#include "config.h"
#include "VideoLayerSelector.h"
#include "h265.h"

class H265LayerSelector : public VideoLayerSelector 
{
public:
	H265LayerSelector();
	virtual ~H265LayerSelector() = default;
	
	void SelectTemporalLayer(BYTE id)		override;
	void SelectSpatialLayer(BYTE id)		override;
	
	bool Select(const RTPPacket::shared& packet,bool &mark)	override;
	
	BYTE GetTemporalLayer()		const override { return temporalLayerId;	}
	BYTE GetSpatialLayer()		const override { return LayerInfo::MaxLayerId;	}
	VideoCodec::Type GetCodec()	const override { return VideoCodec::H265;	}
	bool IsWaitingForIntra()	const override { return waitingForIntra;	}
	
	const H265SeqParameterSet&	GetSeqParameterSet()		const { return sps; }
	const H265PictureParameterSet&	GetPictureParameterSet()	const { return pps; }
	
	static std::vector<LayerInfo> GetLayerIds(const RTPPacket::shared& packet);
private:
	static void SetPacketParameters(const RTPPacket::shared& packet, BYTE nalUnitType, const BYTE* nalData, DWORD nalSize /*includ Nal Header*/);
	bool waitingForIntra;
	H265SeqParameterSet sps;
	H265PictureParameterSet pps;
	BYTE temporalLayerId;
	BYTE nextTemporalLayerId;
private:

};

#endif /* H265LAYERSELECTOR_H */

