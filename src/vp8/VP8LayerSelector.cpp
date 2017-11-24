 #include "VP8LayerSelector.h"
#include "vp8.h"


		
VP8LayerSelector::VP8LayerSelector()
{
	temporalLayerId = 0;
	nextTemporalLayerId = MaxLayerId;
}


void VP8LayerSelector::SelectTemporalLayer(BYTE id)
{
	//Set next
	nextTemporalLayerId = id;
}

void VP8LayerSelector::SelectSpatialLayer(BYTE id)
{
	//Not supported
}
	
bool VP8LayerSelector::Select(RTPPacket *packet,bool &mark)
{
	VP8PayloadDescriptor desc;

	//Parse VP8 payload description
	if (!desc.Parse(packet->GetMediaData(),packet->GetMaxMediaLength()))
		//Error
		return Error("-VP8LayerSelector::Select() | Cannot parse VP8PayloadDescription\n");

	UltraDebug("-tl:%u\t picId:%u\t tl0picIdx:%u\t sync:%u\n",desc.temporalLayerIndex,desc.pictureId,desc.temporalLevelZeroIndex,desc.layerSync);
	
	//Store current temporal id
	BYTE currentTemporalLayerId = temporalLayerId;
	
	//Check if we need to upscale temporally
	if (nextTemporalLayerId>temporalLayerId)
	{
		//Check if we can upscale and it is the start of the layer and it is a valid layer
		if ((!desc.nonReferencePicture || desc.layerSync) && desc.startOfPartition && desc.temporalLayerIndex<=nextTemporalLayerId)
		{
			UltraDebug("-VP8LayerSelector::Select() | Upscaling temporalLayerId [id:%d,target:%d]\n",desc.temporalLayerIndex,nextTemporalLayerId);
			//Update current layer
			temporalLayerId = desc.temporalLayerIndex;
		}
	//Check if we need to downscale
	} else if (nextTemporalLayerId<temporalLayerId) {
		//We can only downscale on the end of a frame
		if (packet->GetMark())
		{
			UltraDebug("-VP8LayerSelector::Select() | Downscaling temporalLayerId [id:%d,target:%d]\n",temporalLayerId,nextTemporalLayerId);
			//Update to target layer
			temporalLayerId = nextTemporalLayerId;
		}
	}
	
	//If it is from the current layer
	if (currentTemporalLayerId<desc.temporalLayerIndex)
	{
		UltraDebug("-VP8LayerSelector::Select() | dropping packet based on temporalLayerId [us:%d,desc:%d,mark:%d]\n",temporalLayerId,desc.temporalLayerIndex,packet->GetMark());
		//Drop it
		return false;
	}
	
	//RTP mark is unchanged
	mark = packet->GetMark();
	
	UltraDebug("-VP8LayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,tid:%d,sid:%d]\n",packet->GetExtSeqNum(),mark,desc.temporalLayerIndex,desc.temporalLayerIndex);
	//Select
	return true;
	
}