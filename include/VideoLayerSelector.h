#ifndef VIDEOLAYERSELECTOR_H
#define VIDEOLAYERSELECTOR_H

#include "config.h"
#include "codecs.h"
#include "rtp.h"

class VideoLayerSelector
{
public:
	static BYTE MaxLayerId;
public:
	virtual void SelectTemporalLayer(BYTE id) = 0;
	virtual void SelectSpatialLayer(BYTE id) = 0;
	virtual bool Select(RTPPacket *packet,bool &mark) = 0;
	
	virtual BYTE GetTemporalLayer()		const = 0;
	virtual BYTE GetSpatialLayer()		const = 0;
	virtual VideoCodec::Type GetCodec()	const = 0;
public:
	//Factory method
	static VideoLayerSelector* Create(VideoCodec::Type codec);
};

#endif /* VIDEOLAYERSELECTOR_H */

