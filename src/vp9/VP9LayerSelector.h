/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   VP9LayerSelector.h
 * Author: Sergio
 *
 * Created on 1 de febrero de 2017, 21:21
 */

#ifndef VP9LAYERSELECTOR_H
#define VP9LAYERSELECTOR_H
#include "config.h"
#include "VideoLayerSelector.h"

class VP9LayerSelector : public VideoLayerSelector
{
public:
	VP9LayerSelector() = default;
	VP9LayerSelector(BYTE temporalLayerId,BYTE spatialLayerId );
	virtual ~VP9LayerSelector() = default;
	void SelectTemporalLayer(BYTE id)		override;
	void SelectSpatialLayer(BYTE id)		override;
	
	bool Select(const RTPPacket::shared& packet,bool &mark)	override;
	
	BYTE GetTemporalLayer()		const override { return temporalLayerId;	}
	BYTE GetSpatialLayer()		const override { return spatialLayerId;		}
	VideoCodec::Type GetCodec()	const override { return VideoCodec::VP9;	}
	bool IsWaitingForIntra()	const override { return false;			}
	
	static std::vector<LayerInfo> GetLayerIds(const RTPPacket::shared& packet);
private:
	bool waitingForIntra = true;
	BYTE temporalLayerId = 0;
	BYTE spatialLayerId = 0;
	BYTE nextTemporalLayerId = LayerInfo::MaxLayerId;
	BYTE nextSpatialLayerId = LayerInfo::MaxLayerId;
};

#endif /* VP9LAYERSELECTOR_H */

