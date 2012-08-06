#include "rtp.h"
#include "audio.h"
#include "h264/h264depacketizer.h"


class DummyAudioDepacketizer : public RTPDepacketizer
{
public:
	DummyAudioDepacketizer(DWORD codec) : RTPDepacketizer(MediaFrame::Audio,codec), frame((AudioCodec::Type)codec)
	{

	}

	virtual ~DummyAudioDepacketizer()
	{

	}

	virtual void SetTimestamp(DWORD timestamp)
	{
		//Set timestamp
		frame.SetTimestamp(timestamp);
	}
	virtual MediaFrame* AddPacket(RTPPacket *packet)
	{
		//Set timestamp
		frame.SetTimestamp(packet->GetTimestamp());
		//Add payload
		return AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	}
	virtual MediaFrame* AddPayload(BYTE* payload,DWORD payload_len)
	{
		//Get current position in frame
		DWORD pos = frame.GetLength();
		//And data
		frame.AppendMedia(payload, payload_len);
		//Add RTP packet
		frame.AddRtpPacket(pos,payload_len,NULL,0);
		//Return it
		return &frame;
	}
	virtual void ResetFrame()
	{
		//Clear packetization info
		frame.ClearRTPPacketizationInfo();
		//Reset
		memset(frame.GetData(),0,frame.GetMaxMediaLength());
		//Clear length
		frame.SetLength(0);
	}
private:
	AudioFrame frame;
};

RTPDepacketizer* RTPDepacketizer::Create(MediaFrame::Type mediaType,DWORD codec)
{
	 switch (mediaType)
	 {
		 case MediaFrame::Video:
			 //Depending on the codec
			 switch((VideoCodec::Type)codec)
			 {
				 case VideoCodec::H264:
					 return new H264Depacketizer();
			 }
			 break;
		 case MediaFrame::Audio:
			 //Dummy depacketizer
			 return new DummyAudioDepacketizer(codec);
			 break;
	 }
	 return NULL;
}
