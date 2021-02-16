#include "opusdepacketizer.h"

MediaFrame* OpusDepacketizer::AddPacket(const RTPPacket::shared& packet)
{
	//Reset frame
	ResetFrame();
	//Set timestamp
	frame.SetTimestamp(packet->GetExtTimestamp());
	//Set clock rate
	frame.SetClockRate(packet->GetClockRate());
	//Set time
	frame.SetTime(packet->GetTime());
	//Set sender time
	frame.SetSenderTime(packet->GetSenderTime());
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
	
	//Get opus toc
	auto [mode, bandwidth, frameSize, stereo, codeNumber ] = OpusTOC::TOC(payload[0]);
	
	//Set number of channels
	uint8_t numChannels = stereo ? 2 : 1;

	//Set it
	frame.SetNumChannels(numChannels);
	
	//Set duration
	frame.SetDuration(frameSize);
	
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
	
