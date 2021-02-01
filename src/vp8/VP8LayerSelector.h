/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   VP8LayerSelector.h
 * Author: Sergio
 *
 * Created on 7 de noviembre de 2017, 19:57
 */

#ifndef VP8LAYERSELECTOR_H
#define VP8LAYERSELECTOR_H

#include "config.h"
#include "VideoLayerSelector.h"

class VP8LayerSelector : public VideoLayerSelector
{
public:
	VP8LayerSelector();
	virtual ~VP8LayerSelector() = default;
	
	void SelectTemporalLayer(BYTE id)		override;
	void SelectSpatialLayer(BYTE id)		override;
	
	bool Select(const RTPPacket::shared& packet,bool &mark)	override;
	
	BYTE GetTemporalLayer()		const override { return temporalLayerId;	}
	BYTE GetSpatialLayer()		const override { return LayerInfo::MaxLayerId;	}
	VideoCodec::Type GetCodec()	const override { return VideoCodec::VP8;	}
	bool IsWaitingForIntra()	const override { return waitingForIntra;	}
	
	static std::vector<LayerInfo> GetLayerIds(const RTPPacket::shared& packet);
	
private:
	bool waitingForIntra;
	BYTE temporalLayerId;
	BYTE nextTemporalLayerId;
};

#endif /* VP8LAYERSELECTOR_H */

