
#include "VideoLayerSelector.h"
#include "vp9/VP9LayerSelector.h"
#include "vp8/VP8LayerSelector.h"
#include "h264/H264LayerSelector.h"
#include "av1/AV1LayerSelector.h"


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
			return new AV1LayerSelector();
		default:
			return new DummyVideoLayerSelector(codec);
	}
}

std::vector<LayerInfo> VideoLayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	switch(packet->GetCodec())
	{
		case VideoCodec::VP9:
			return VP9LayerSelector::GetLayerIds(packet);
		case VideoCodec::VP8:
			return VP8LayerSelector::GetLayerIds(packet);
		case VideoCodec::H264:
			return H264LayerSelector::GetLayerIds(packet);
		case VideoCodec::AV1:
			return AV1LayerSelector::GetLayerIds(packet);
		default:
			return {};
	}
}


 bool VideoLayerSelector::AreLayersInfoeAggregated(const RTPPacket::shared& packet)
 {
	 return packet->GetCodec()== VideoCodec::AV1;
 }