#ifndef RTPPACKETIZER_H
#define RTPPACKETIZER_H
#include "config.h"
#include "media.h"
#include "rtp/RTPPacket.h"

class RTPPacketizer
{
public:
	static RTPPacketizer* Create(MediaFrame::Type mediaType, DWORD codec);

public:
	RTPPacketizer(MediaFrame::Type mediaType, DWORD codec)
	{
		//Store
		this->mediaType = mediaType;
		this->codec = codec;
	}
	virtual ~RTPPacketizer() {};

	MediaFrame::Type GetMediaType()	const { return mediaType; }
	DWORD		 GetCodec()	const { return codec; }

	virtual std::unique_ptr<MediaFrame> ProcessAU(BufferReader& accessUnit) = 0;
protected:
	MediaFrame::Type	mediaType;
	DWORD				codec;
};


#endif /* RTPPACKETIZER_H */