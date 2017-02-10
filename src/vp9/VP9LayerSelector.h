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
#include "rtp.h"

class VP9LayerSelector
{
public:
	static BYTE MaxLayerId;
public:
	VP9LayerSelector();
	VP9LayerSelector(BYTE temporalLayerId,BYTE spatialLayerId );
	void SelectTemporalLayer(BYTE id);
	void SelectSpatialLayer(BYTE id);
	
	bool Select(RTPPacket *packet,DWORD &extSeqNum,bool &mark);
	
	BYTE GetTemporalLayer() const	{ return temporalLayerId; }
	BYTE GetSpatialLayer()	const	{ return spatialLayerId;  }
	
private:
	BYTE temporalLayerId;
	BYTE spatialLayerId;
	BYTE nextTemporalLayerId;
	BYTE nextSpatialLayerId;
	DWORD dropped;
};

#endif /* VP9LAYERSELECTOR_H */

