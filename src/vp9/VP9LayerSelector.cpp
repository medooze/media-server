/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   VP9LayerSelector.cpp
 * Author: Sergio
 * 
 * Created on 1 de febrero de 2017, 21:21
 */

#include "VP9LayerSelector.h"
#include "VP9PayloadDescription.h"

BYTE VP9LayerSelector::MaxLayerId = 0xFF;

VP9LayerSelector::VP9LayerSelector()
{
	temporalLayerId = 0;
	spatialLayerId  = 0;
	nextTemporalLayerId = MaxLayerId;
	nextSpatialLayerId = MaxLayerId;
	dropped = 0;
}

VP9LayerSelector::VP9LayerSelector(BYTE temporalLayerId,BYTE spatialLayerId )
{
	this->temporalLayerId = temporalLayerId;
	this->spatialLayerId  = spatialLayerId;
	nextTemporalLayerId = temporalLayerId;
	nextSpatialLayerId = spatialLayerId;
	dropped = 0;
}

void VP9LayerSelector::SelectTemporalLayer(BYTE id)
{
	//Set next
	nextTemporalLayerId = id;
}

void VP9LayerSelector::SelectSpatialLayer(BYTE id)
{
	//Set next
	nextSpatialLayerId = id;
}
	
bool VP9LayerSelector::Select(RTPPacket *packet,DWORD &extSeqNum,bool &mark)
{
	VP9PayloadDescription desc;
	
	//Parse VP9 payload description
	if (!desc.Parse(packet->GetMediaData(),packet->GetMaxMediaLength()))
		//Error
		return Error("-VP9LayerSelector::Select() | Cannot parse VP9PayloadDescription\n");
	
	//Check if we need to upscale temporally
	if (nextTemporalLayerId>temporalLayerId)
	{
		//Check if we can upscale and it is the start of the layer and it is a valid layer
		if (desc.switchingPoint && desc.startOfLayerFrame && desc.temporalLayerId<=nextTemporalLayerId)
		{
			UltraDebug("-VP9LayerSelector::Select() | Upscaling temporalLayerId [id:%d,target:%d]\n",desc.temporalLayerId,nextTemporalLayerId);
			//Update current layer
			temporalLayerId = desc.temporalLayerId;
		}
	//Check if we need to downscale
	} else if (nextTemporalLayerId<temporalLayerId) {
		//We can only downscale on the end of a layer to set the market bit
		if (desc.endOfLayerFrame)
		{
			UltraDebug("-VP9LayerSelector::Select() | Downscaling temporalLayerId [id:%d,target:%d]\n",temporalLayerId,nextTemporalLayerId);
			//Update to target layer
			temporalLayerId = nextTemporalLayerId;
		}
	}
	
	//If it is from the current layer
	if (temporalLayerId<desc.temporalLayerId)
	{
		UltraDebug("-VP9LayerSelector::Select() | dropping packet based on temporalLayerId [us:%d,desc:%d,mark:%d]\n",temporalLayerId,desc.temporalLayerId,packet->GetMark());
		//INcrease dropped
		dropped++;
		//Drop it
		return false;
	}
	
	//Check if we need to upscale spatially
	if (nextSpatialLayerId>spatialLayerId)
	{
		//Check if we can upscale and it is the start of the layer and it is a valid layer
		if (desc.switchingPoint && desc.startOfLayerFrame && desc.spatialLayerId<=nextSpatialLayerId)
		{
			UltraDebug("-VP9LayerSelector::Select() | Upscaling spatialLayerId [ [id:%d,target:%d]\n",desc.spatialLayerId,nextSpatialLayerId);
			//Update current layer
			spatialLayerId = desc.spatialLayerId;
		}
	//Ceck if we need to downscale
	} else if (nextSpatialLayerId<spatialLayerId) {
		//We can only downscale on the end of a layer to set the market bit
		if (desc.endOfLayerFrame)
		{
			UltraDebug("-VP9LayerSelector::Select() | Downscaling spatialLayerId [id:%d,target:%d]\n",spatialLayerId,nextSpatialLayerId);
			//Update to target layer
			spatialLayerId = nextSpatialLayerId;
		}
	}
	
	//If it is from the current layer
	if (spatialLayerId<desc.spatialLayerId)
	{
		UltraDebug("-VP9LayerSelector::Select() | dropping packet based on spatialLayerId [us:%d,desc:%d,mark:%d]\n",spatialLayerId,desc.spatialLayerId,packet->GetMark());
		//INcrease dropped
		dropped++;
		//Drop it
		return false;
	}
		
	
	//Calculate new packet number removing the dropped pacekts by the selection layer
	extSeqNum = packet->GetExtSeqNum()-dropped;
	
	//RTP mark is set for the last frame layer of the selected layer
	mark = packet->GetMark() || (desc.endOfLayerFrame && spatialLayerId==desc.spatialLayerId);
	
	UltraDebug("-VP9LayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,tid:%d,sid:%d]\n",extSeqNum,mark,desc.temporalLayerId,desc.spatialLayerId);
	//Select
	return true;
	
}
