#include "opusdepacketizer.h"

MediaFrame* OpusDepacketizer::AddPacket(const RTPPacket::shared& packet)
{
	//Reset frame
	ResetFrame();
	//Set timestamp
	frame.SetTimestamp(packet->GetTimestamp());
	//Set SSRC
	frame.SetSSRC(packet->GetSSRC());
	//Add payload
	AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	//Return frame
	return &frame;
}

MediaFrame* OpusDepacketizer::AddPayload(const BYTE* payload,DWORD payloadLen)
{
	//Check lenght
	if (!payloadLen)
		return nullptr;
	//And data
	DWORD pos = frame.AppendMedia(payload, payloadLen);
	//Add RTP packet
	frame.AddRtpPacket(pos,payloadLen,NULL,0);
	
	/*
	 *
		0 1 2 3 4 5 6 7
	       +-+-+-+-+-+-+-+-+
	       | config  |s| c |
	       +-+-+-+-+-+-+-+-+
	 *
	 */
	//Get opus toc
	uint8_t toc = payload[0];
	
	//Set number of channels
	uint8_t numChannels = toc & 0x04 ? 2 : 1;
	
	//Check if we have to change config
	if (!frame.HasCodecConfig() || numChannels!=config.GetOutputChannelCount())
	{
		uint8_t data[19];
		//Update channels
		config.SetOutputChannelCount(numChannels);
		//Serialize config
		auto len = config.Serialize(data,sizeof(data));
		//Set it in config
		frame.SetCodecConfig(data,len);
	}
	//Return it
	return &frame;
}

void OpusDepacketizer::ResetFrame()
{
	//Reset frame data
	frame.Reset();
}
	
