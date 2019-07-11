#ifndef VIDEOLAYERSELECTOR_H
#define VIDEOLAYERSELECTOR_H

#include "config.h"
#include "codecs.h"
#include "rtp.h"

class VideoLayerSelector
{
public:
	virtual ~VideoLayerSelector() = default;
	virtual void SelectTemporalLayer(BYTE id) = 0;
	virtual void SelectSpatialLayer(BYTE id) = 0;
	virtual bool Select(const RTPPacket::shared& packet,bool &mark) = 0;
	
	virtual BYTE GetTemporalLayer()		const = 0;
	virtual BYTE GetSpatialLayer()		const = 0;
	virtual VideoCodec::Type GetCodec()	const = 0;
	
public:
	//Factory method
	static VideoLayerSelector* Create(VideoCodec::Type codec);
	static LayerInfo GetLayerIds(const RTPPacket::shared& packet);

public:
	//Common method
	void SetWaitingForIntra(bool waitingForIntra);

protected: 
	bool waitingForIntra;
};

#endif /* VIDEOLAYERSELECTOR_H */

