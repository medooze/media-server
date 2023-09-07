
#include "VideoLayerSelector.h"
#include "vp9/VP9LayerSelector.h"
#include "vp8/VP8LayerSelector.h"
#include "h264/H264LayerSelector.h"
#include "DependencyDescriptorLayerSelector.h"


class DummyVideoLayerSelector : public VideoLayerSelector
{
public:
	DummyVideoLayerSelector(VideoCodec::Type codec)
	{
		this->codec = codec;
	}
	
	void SelectTemporalLayer(BYTE id) override {};
	void SelectSpatialLayer(BYTE id) override {};
	
	bool Select(const RTPPacket::shared& packet,bool &mark)	override 
	{
		mark = packet->GetMark();
		return true;
	}
	bool IsWaitingForIntra() const override	{ return false; }
	BYTE GetTemporalLayer() const override { return LayerInfo::MaxLayerId;  }
	BYTE GetSpatialLayer()	const override { return LayerInfo::MaxLayerId;  }
	VideoCodec::Type GetCodec()	const override { return codec; }
private:
	VideoCodec::Type codec;
	
};

VideoLayerSelector* VideoLayerSelector::Create(VideoCodec::Type codec)
{
	Debug("VideoLayerSelector::Create() [codec:%s]\n",VideoCodec::GetNameFor(codec));

	switch(codec)
	{
		case VideoCodec::VP9:
			return new VP9LayerSelector();
		case VideoCodec::VP8:
			return new VP8LayerSelector();
		case VideoCodec::H264:
			return new H264LayerSelector();
		case VideoCodec::AV1:
			return new DependencyDescriptorLayerSelector(VideoCodec::AV1);
		default:
			return new DummyVideoLayerSelector(codec);
	}
}

 std::vector<LayerInfo> VideoLayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	 std::vector<LayerInfo> layerInfo;

	switch(packet->GetCodec())
	{
		case VideoCodec::VP9:
			layerInfo = VP9LayerSelector::GetLayerIds(packet);
			break;
		case VideoCodec::VP8:
			layerInfo = VP8LayerSelector::GetLayerIds(packet);
			break;
		case VideoCodec::H264:
			layerInfo = H264LayerSelector::GetLayerIds(packet);
			break;
		case VideoCodec::AV1:
			layerInfo = DependencyDescriptorLayerSelector::GetLayerIds(packet);
			break;
		default:
			break;
	}

	//If packet has VLA header extension 
	if (packet->HasVideoLayersAllocation())
	{
		//Get vla info
		const auto& videoLayersAllocation = packet->GetVideoLayersAllocation();
		//Deactivate all layers
		for (auto& layer: layerInfo)
			layer.active = false;
		//For each active layer
		for (const auto& activeLayer : videoLayersAllocation->activeSpatialLayers)
		{
			//IF it is from us
			if (activeLayer.streamIdx == videoLayersAllocation->streamIdx)
			{
				bool found = false;
				//Find layer
				for (auto& layer: layerInfo)
				{
					//if found
					if (layer.spatialLayerId == activeLayer.spatialId)
					{
						//It is active
						layer.active = true;
						//Get bitrate for temporal layer
						layer.targetBitrate = activeLayer.targetBitratePerTemporalLayer[layer.temporalLayerId];
						//Set dimensios for the spatial layer
						layer.targetWidth = activeLayer.width;
						layer.targetHeight = activeLayer.height;
						layer.targetFps = activeLayer.fps;
						//Layer found
						found = true;
					}
				}
				//If layer was not found on the layer info or ther was no layer info
				if (!found)
				{
					//For each temporal layer
					for (auto temporaLayerId = 0; temporaLayerId < activeLayer.targetBitratePerTemporalLayer.size(); ++temporaLayerId)
					{
						//Create new Layer
						LayerInfo layer(activeLayer.spatialId, temporaLayerId);
						//It is active
						layer.active = true;
						//Get bitrate for temporal layer
						layer.targetBitrate = activeLayer.targetBitratePerTemporalLayer[layer.temporalLayerId];
						//Set dimensios for the spatial layer
						layer.targetWidth = activeLayer.width;
						layer.targetHeight = activeLayer.height;
						layer.targetFps = activeLayer.fps;
						//Append to layers
						layerInfo.push_back(layer);
					}
				}
			}
		}
	}
	
	return layerInfo;
}


 bool VideoLayerSelector::AreLayersInfoeAggregated(const RTPPacket::shared& packet)
 {
	 return packet->GetCodec()== VideoCodec::AV1;
 }