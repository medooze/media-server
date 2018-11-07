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

struct LayerInfo
{
	static BYTE MaxLayerId; 
	BYTE temporalLayerId = MaxLayerId;
	BYTE spatialLayerId  = MaxLayerId;
	
	bool IsValid() { return spatialLayerId!=MaxLayerId || temporalLayerId != MaxLayerId;	}
	WORD GetId()   { return ((WORD)spatialLayerId)<<8  | temporalLayerId;			}
};

struct LayerSource : LayerInfo
{
	DWORD		numPackets = 0;
	DWORD		totalBytes = 0;
	DWORD		bitrate;
	Acumulator	acumulator;
	
	LayerSource() : acumulator(1000)
	{
		
	}
	
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
};

struct RTPSource 
{
	DWORD	ssrc;
	DWORD   extSeqNum;
	DWORD	cycles;
	DWORD	jitter;
	DWORD	numPackets;
	DWORD	numRTCPPackets;
	DWORD	totalBytes;
	DWORD	totalRTCPBytes;
	DWORD   bitrate;
	Acumulator acumulator;
	
	RTPSource() : acumulator(1000)
	{
		ssrc		= 0;
		extSeqNum	= 0;
		cycles		= 0;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
		jitter		= 0;
		bitrate		= 0;
	}
	
	virtual ~RTPSource()
	{
		
	}
	
	RTCPCompoundPacket::shared CreateSenderReport();
	
	WORD SetSeqNum(WORD seqNum)
	{
		//Check if we have a sequence wrap
		if (seqNum<0x0FFF && (extSeqNum & 0xFFFF)>0xF000)
			//Increase cycles
			cycles++;

		//Get ext seq
		DWORD extSeqNum = ((DWORD)cycles)<<16 | seqNum;

		//If we have a not out of order pacekt
		if (extSeqNum > this->extSeqNum || !numPackets)
			//Update seq num
			this->extSeqNum = extSeqNum;
		
		//Return seq cycles count
		return cycles;
	}
	
	void SetExtSeqNum(DWORD extSeqNum )
	{
		//Updte seqNum and cycles
		this->extSeqNum = extSeqNum;
		cycles = extSeqNum >>16;
		
	}

	
	/*
	 * Get seq num cycles from a past sequence numer
	 */
	WORD RecoverSeqNum(WORD osn)
	{
		 //Check secuence wrap
		if ((extSeqNum & 0xFFFF)<0x0FFF && (osn>0xF000))
			//It is from the past cycle
			return cycles - 1;
		//It is from current cyle
		return cycles;
	}
	
	virtual void Update(QWORD now, DWORD seqNum,DWORD size) 
	{
		//Increase stats
		numPackets++;
		totalBytes += size;
		
		//Update bitrate acumulator
		acumulator.Update(now,size);
		
		//Get bitrate in bps
		bitrate = acumulator.GetInstant()*8;
	}
	
	virtual void Reset()
	{
		ssrc		= 0;
		extSeqNum	= 0;
		cycles		= 0;
		numPackets	= 0;
		numRTCPPackets	= 0;
		totalBytes	= 0;
		totalRTCPBytes	= 0;
		jitter		= 0;
		bitrate		= 0;
	}
};


#endif /* RTPSOURCE_H */

