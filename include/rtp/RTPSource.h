/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPSource.h
 * Author: Sergio
 *
 * Created on 13 de julio de 2018, 11:49
 */

#ifndef RTPSOURCE_H
#define RTPSOURCE_H

#include "config.h"
#include "acumulator.h"
#include "rtp/RTCPSenderReport.h"
#include "rtp/RTCPCompoundPacket.h"

#include <optional>

struct LayerSource : LayerInfo
{
	DWORD		numPackets = 0;
	QWORD		totalBytes = 0;
	DWORD		bitrate = 0;

	Acumulator<uint32_t, uint64_t>	acumulator;

	// From VideoLayerAllocation info
	bool active = true;
	std::optional<uint32_t> targetBitrate;
	std::optional<uint16_t> targetWidth;
	std::optional<uint16_t> targetHeight;
	std::optional<uint8_t>  targetFps;
	
	LayerSource() : acumulator(1000)
	{
		
	}
	virtual ~LayerSource() = default;
	
	LayerSource(const LayerInfo& layerInfo) : acumulator(1000)
	{
		spatialLayerId  = layerInfo.spatialLayerId;
		temporalLayerId = layerInfo.temporalLayerId; 
	}
	
	void Update(QWORD now, DWORD size) 
	{
		//Increase stats
		numPackets++;
		totalBytes += size;
		
		//Update bitrate acumulator
		acumulator.Update(now,size);
		
		//Update bitrate in bps
		bitrate = acumulator.GetInstant()*8;
	}
	
	virtual void Update(QWORD now)
	{
		//Update bitrate acumulator
		acumulator.Update(now);
		//Update bitrate in bps
		bitrate = acumulator.GetInstant()*8;
	}
};

struct RTPSource
{
	DWORD	ssrc = 0;
	DWORD   extSeqNum = 0;
	DWORD	cycles = 0;
	DWORD	jitter = 0;
	DWORD	numPackets = 0;
	DWORD   numPacketsDelta = 0;
	DWORD	numRTCPPackets = 0;
	QWORD	totalBytes = 0;
	QWORD	totalRTCPBytes = 0;
	DWORD   bitrate = 0;
	DWORD   totalBitrate = 0;
	DWORD	clockrate = 0;
	Acumulator<uint32_t, uint64_t> acumulator;
	Acumulator<uint32_t, uint64_t> acumulatorTotalBitrate;
	Acumulator<uint32_t, uint64_t> acumulatorPackets;
	
	RTPSource();
	virtual ~RTPSource() = default;
	
	WORD SetSeqNum(WORD seqNum);
	void SetExtSeqNum(DWORD extSeqNum );
	virtual void Update(QWORD now, DWORD seqNum,DWORD size, DWORD overheadSize);
	virtual void Update(QWORD now);
	virtual void Reset();
};


#endif /* RTPSOURCE_H */

