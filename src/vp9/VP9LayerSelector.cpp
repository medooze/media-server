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
#include "VP9.h"


VP9LayerSelector::VP9LayerSelector(BYTE temporalLayerId,BYTE spatialLayerId )
{
	nextTemporalLayerId	= temporalLayerId;
	nextSpatialLayerId	= spatialLayerId;
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
	
bool VP9LayerSelector::Select(const RTPPacket::shared& packet,bool &mark)
{
	//Get VP9 payload description
	if (!packet->vp9PayloadDescriptor) 
		return Error("-VP9LayerSelector::Select() | coulnd't retrieve VP9PayloadDescription\n");
	
	//Get description
	auto& desc = *packet->vp9PayloadDescriptor;
	
	//if (desc.startOfLayerFrame)
	//	//UltraDebug("-VP9LayerSelector::Select() | s:%d end:%d #%d T%dS%d P=%d D=%d S=%d %s\n", desc.startOfLayerFrame, desc.endOfLayerFrame, desc.pictureId,desc.temporalLayerId,desc.spatialLayerId,desc.interPicturePredictedLayerFrame,desc.interlayerDependencyUsed,desc.switchingPoint
//			,desc.interPicturePredictedLayerFrame==0 && desc.spatialLayerId==1 ? "<----------------------":"");
	
	//Store current temporal id
	BYTE currentTemporalLayerId = temporalLayerId;
	
	//Check if we need to upscale temporally
	if (nextTemporalLayerId>temporalLayerId)
	{
		//Check if we can upscale and it is the start of the layer and it is a valid layer
		if (desc.switchingPoint && desc.startOfLayerFrame && currentTemporalLayerId<desc.temporalLayerId && desc.temporalLayerId<=nextTemporalLayerId)
		{
			//UltraDebug("-VP9LayerSelector::Select() | Upscaling temporalLayerId [id:%d,target:%d]\n",desc.temporalLayerId,nextTemporalLayerId);
			//Update current layer
			currentTemporalLayerId = temporalLayerId = desc.temporalLayerId;
		}
	//Check if we need to downscale
	} else if (nextTemporalLayerId<temporalLayerId) {
		//We can only downscale on the end of a layer to set the market bit
		if (desc.endOfLayerFrame)
		{
			//UltraDebug("-VP9LayerSelector::Select() | Downscaling temporalLayerId [id:%d,target:%d]\n",desc.temporalLayerId,nextTemporalLayerId);
			//Update to target layer for next packets
			temporalLayerId = desc.temporalLayerId;
		}
	}
	
	//If it is from a higher layers
	if (desc.temporalLayerId>currentTemporalLayerId)
	{
		//UltraDebug("-VP9LayerSelector::Select() | dropping packet based on temporalLayerId [us:%d,desc:%d,mark:%d]\n",temporalLayerId,desc.temporalLayerId,packet->GetMark());
		//Drop it
		return false;
	}
	
	//Get current spatial layer
	BYTE currentSpatialLayerId = spatialLayerId;
	
	//Check if we need to upscale spatially
	if (nextSpatialLayerId>spatialLayerId)
	{
		/*
			Inter-picture predicted layer frame.  When set to zero, the layer
			frame does not utilize inter-picture prediction.  In this case,
			up-switching to current spatial layer's frame is possible from
			directly lower spatial layer frame.  P SHOULD also be set to zero
			when encoding a layer synchronization frame in response to an LRR
		 */
		//Check if we can upscale and it is the start of the layer and it is a valid layer
		if (desc.interPicturePredictedLayerFrame==0 && desc.startOfLayerFrame && currentSpatialLayerId<desc.spatialLayerId && desc.spatialLayerId<=nextSpatialLayerId)
		{
			//UltraDebug("-VP9LayerSelector::Select() | Upscaling spatialLayerId [id:%d,to:%d,target:%d]\n",desc.spatialLayerId,spatialLayerId,nextSpatialLayerId);
			//Update current layer
			currentSpatialLayerId = spatialLayerId = desc.spatialLayerId;
			
		}
	//Ceck if we need to downscale
	} else if (nextSpatialLayerId<spatialLayerId) {
		//We can only downscale on the end of a layer to set the market bit
		if (desc.endOfLayerFrame)
		{
			//UltraDebug("-VP9LayerSelector::Select() | Downscaling spatialLayerId [id:%d,to:%d,target:%d]\n",desc.spatialLayerId,spatialLayerId,nextSpatialLayerId);
			//Update to target layer
			spatialLayerId = desc.spatialLayerId;
		}
	}
	
	//If it is from a higher layers
	if (desc.spatialLayerId>currentSpatialLayerId)
	{
		//UltraDebug("-VP9LayerSelector::Select() | dropping packet based on spatialLayerId [us:%d,desc:%d,mark:%d]\n",spatialLayerId,desc.spatialLayerId,packet->GetMark());
		//Drop it
		return false;
	}
	
	//If we have to wait for first intra
	if (waitingForIntra)
	{
		//If this is not intra
		if (!desc.interPicturePredictedLayerFrame)
			//Discard
			return false;
		//Stop waiting
		waitingForIntra = 0;
	}
	
	//RTP mark is set for the last frame layer of the selected layer
	mark = packet->GetMark() || (desc.endOfLayerFrame && spatialLayerId==desc.spatialLayerId && nextSpatialLayerId<=spatialLayerId);
	
	//UltraDebug("-VP9LayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,sid:%d,tid:%d,current:S%dL%d]\n",packet->GetExtSeqNum(),mark,desc.spatialLayerId,desc.temporalLayerId,spatialLayerId,temporalLayerId);
	//Select
	return true;
	
}

 std::vector<LayerInfo> VP9LayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	 std::vector<LayerInfo> infos;
	
	//If we don't have one yet
	if (!packet->vp9PayloadDescriptor)
	{
		//Create new one
		auto& desc = packet->vp9PayloadDescriptor.emplace();
			
		//If packet has frame markings
		if (packet->HasFrameMarkings())
		{
			//Get it from frame markings
			const auto& fm = packet->GetFrameMarks();
			//Import data
			desc.pictureIdPresent			= false;
			desc.layerIndicesPresent		= 0;
			desc.flexibleMode			= 0;
			desc.startOfLayerFrame			= fm.startOfFrame;
			desc.endOfLayerFrame			= fm.endOfFrame;
			desc.scalabiltiyStructureDataPresent	= 0;
			desc.pictureId				= 0;
			desc.temporalLayerId			= fm.temporalLayerId;
			// The following  shows VP9 Layer encoding information (3 bits for
			// spatial and temporal layer) mapped to the generic LID and TID fields.
			// The P and U bits MUST match the corresponding bits in the VP9 Payload
			// Description.
			//    0                
			//    0 1 2 3 4 5 6 7
			//   +-+-+-+-+-+-+-+-+
			//   |0|0|0|P|U| SID |
			//   +-+-+-+-+-+-+-+-+
			desc.interPicturePredictedLayerFrame	= fm.layerId & 0x20;
			desc.switchingPoint			= fm.layerId & 0x10;
			desc.spatialLayerId			= fm.layerId & 0x07;
			desc.interlayerDependencyUsed		= false;
			desc.temporalLayer0Index		= fm.tl0PicIdx;
		//We need to parse it
		} else if (packet->GetMediaLength() && !desc.Parse(packet->GetMediaData(),packet->GetMediaLength())) {
			Error("-VP9LayerSelector::GetLayerIds() | parse error\n");
		}
		//Set key fram
		packet->SetKeyFrame(!desc.interPicturePredictedLayerFrame);
	}

	//Check if we have it now
	if (packet->vp9PayloadDescriptor)
	{
		//Get data from header
		infos.emplace_back(packet->vp9PayloadDescriptor->temporalLayerId,packet->vp9PayloadDescriptor->spatialLayerId);
		
		auto& desc = packet->vp9PayloadDescriptor.value();
		if (desc.scalabiltiyStructureDataPresent && desc.scalabilityStructure.spatialLayerFrameResolutionPresent && desc.layerIndicesPresent &&
			desc.spatialLayerId < desc.scalabilityStructure.spatialLayerFrameResolutions.size())
		{
			auto& resolution = desc.scalabilityStructure.spatialLayerFrameResolutions[desc.spatialLayerId];
			packet->SetWidth(resolution.first);
			packet->SetHeight(resolution.second);
		}
		
			
		// We will parse the resolution only if it is not set yet
		if ((packet->GetWidth() == 0 || packet->GetHeight() == 0) && desc.startOfLayerFrame && packet->GetMediaLength() > desc.GetSize())
		{
			VP9FrameHeader frameHeader;
			if (frameHeader.Parse(packet->GetMediaData() + desc.GetSize(), packet->GetMediaLength() - desc.GetSize()))
			{
				if (frameHeader.GetFrameWidthMinus1().has_value() && frameHeader.GetFrameHeightMinus1().has_value())
				{
					packet->SetWidth(*frameHeader.GetFrameWidthMinus1() + 1);
					packet->SetHeight(*frameHeader.GetFrameHeightMinus1() + 1);
				}
			}
			else
			{
				Warning("-VP9LayerSelector::GetLayerIds() | parse frame header error. Size: %d\n", packet->GetMediaLength() - desc.GetSize());
			}
		}
	} else if (packet->GetMaxMediaLength()) { 
		Error("-VP9LayerSelector::GetLayerIds() | no descriptor");
	}
	
	//UltraDebug("-VP9LayerSelector::GetLayerIds() | [tid:%u,sid:%u]\n",info.temporalLayerId,info.spatialLayerId);
	
	//Return layer info
	return infos;
}
