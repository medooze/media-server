#ifndef AV1DEPACKETIZER_H
#define	AV1DEPACKETIZER_H
#include "rtp.h"
#include "video.h"

class ByteReader;

class AV1Depacketizer : public RTPDepacketizer
{
public:
	AV1Depacketizer();
	virtual ~AV1Depacketizer();
	virtual MediaFrame* AddPacket(const RTPPacket::shared& packet) override;
	virtual MediaFrame* AddPayload(const BYTE* payload,DWORD payload_len) override;
	virtual void ResetFrame() override;
private:
	void AddObu(BufferReader& obu);
private:
	Buffer fragment;
	VideoFrame frame;
	LayerFrame layer;
};

#endif	/* AV1DEPACKETIZER_H */

