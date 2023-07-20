
#include "rtmp/rtmppacketizer.h"


VideoCodec::Type RtmpVideoPacketizer::GetRtmpFrameVideoCodec(const RTMPVideoFrame& videoFrame)
{
	if (!videoFrame.IsExtended())
	{
		switch (videoFrame.GetVideoCodec())
		{
		case RTMPVideoFrame::AVC:
			return VideoCodec::H264;
		default:
			return VideoCodec::UNKNOWN;
		}
	}
	
	switch (videoFrame.GetVideoCodecEx())
	{
	case RTMPVideoFrame::HEVC: 
		return VideoCodec::H265;
	
	case RTMPVideoFrame::AV1: 
		return VideoCodec::AV1;
	
	case RTMPVideoFrame::VP9: 
		return VideoCodec::VP9;
		
	default:
		return VideoCodec::UNKNOWN;
	}
}

RtmpVideoPacketizer::RtmpVideoPacketizer(VideoCodec::Type codec, std::unique_ptr<CodecConfigurationRecord> codecConfig) : 
	RawFrameProcessor(),
	codec(codec), 
	codecConfig(std::move(codecConfig)) 
{
}

std::unique_ptr<MediaFrame> RtmpVideoPacketizer::AddFrame(RawFrame* rawFrame)
{
	//Check it is processing codec
	if (GetRtmpFrameVideoCodec(*static_cast<RTMPVideoFrame*>(rawFrame))!= codec)
		//Ignore
		return nullptr;
	
	//Check if it is descriptor
	if (rawFrame->GetRawMediaType() == RawFrameMediaType::Config)
	{
		//Parse it
		if(codecConfig->Parse(rawFrame->GetMediaData(),rawFrame->GetMediaSize()))
		{
			//Got config
			gotConfig = true;
		}
		else
			//Show error
			Error(" RTMPAVCPacketizer::AddFrame() | AVCDescriptor parse error\n");
		//DOne
		return nullptr;
	}
	
	//It should be a nalu then
	if (rawFrame->GetRawMediaType() != RawFrameMediaType::CodedFrames)
		//DOne
		return nullptr;
	
	//Ensure that we have got config
	if (!gotConfig)
	{
		//Error
		Debug("-RTMPAVCPacketizer::AddFrame() | Gor NAL frame but not valid description yet\n");
		//DOne
		return nullptr;
	}
	
	//Create frame
	auto frame = std::make_unique<VideoFrame>(codec,rawFrame->GetSize()+codecConfig->GetSize()+256);
	
	//Set time
	frame->SetTime(rawFrame->GetTimestamp());
	//Set clock rate
	frame->SetClockRate(1000);
	//Set timestamp
	frame->SetTimestamp(rawFrame->GetTimestamp());
	
	//Get AVC data size
	auto configSize = codecConfig->GetSize();
	//Allocate mem
	BYTE* config = (BYTE*)malloc(configSize);
	//Serialize AVC codec data
	DWORD configLen = codecConfig->Serialize(config,configSize);
	//Set it
	frame->SetCodecConfig(config,configLen);
	//Free data
	free(config);
	
	frame->SetWidth(frameWidth);
	frame->SetHeight(frameHeight);
	
	DWORD nalUnitLength = codecConfig->GetNALUnitLengthSizeMinus1() + 1;
	
	auto appender = FrameMediaAppender::Create(*frame, nalUnitLength);
				
	//If is an intra
	if (rawFrame->IsKeyFrame())
	{
		codecConfig->Map([&appender](const uint8_t* data, size_t size) {
			appender->AppendUnit(data, size);
		});
		
		(void)codecConfig->GetResolution(frameWidth, frameHeight);
		
		//Set intra flag
		frame->SetIntra(true);
	}

	//Malloc
	BYTE *data = rawFrame->GetMediaData();
	//Get size
	DWORD size = rawFrame->GetMediaSize();
	
	//Chop into NALs
	while(size>nalUnitLength)
	{
		//Get size
		DWORD nalSize = getN(nalUnitLength, data, 0);
		
		//Ensure we have enougth data
		if (nalSize+nalUnitLength>size)
		{
			//Error
			Error("-RTMPAVCPacketizer::AddFrame() Error adding size=%d nalSize=%d fameSize=%d\n",size,nalSize,rawFrame->GetMediaSize());
			//Skip
			break;
		}

		//Get NAL start
		BYTE* nal = data + nalUnitLength;

		//Log("-NAL %x size=%d nalSize=%d fameSize=%d\n",nal[0],size,nalSize,rawFrame->GetMediaSize());
		
		appender->AppendUnit(nal,nalSize);
		
		//Skip it
		data+=nalUnitLength+nalSize;
		size-=nalUnitLength+nalSize;
	}
	//Done
	return frame;
}
	
std::unique_ptr<AudioFrame> RTMPAACPacketizer::AddFrame(RTMPAudioFrame* audioFrame)
{
	//Debug("-RTMPAACPacketizer::AddFrame() [size:%u,aac:%d,codec:%d]\n",audioFrame->GetMediaSize(),audioFrame->GetAACPacketType(),audioFrame->GetAudioCodec());
	
	if (audioFrame->GetAudioCodec()!=RTMPAudioFrame::AAC)
	{
		Debug("-RTMPAACPacketizer::AddFrame() | Skip non-AAC codec\n");
		return nullptr;
	}
	
	//Check if it is AAC descriptor
	if (audioFrame->GetAACPacketType()==RTMPAudioFrame::AACSequenceHeader)
	{
		//Handle specific settings
		gotConfig = aacSpecificConfig.Decode(audioFrame->GetMediaData(),audioFrame->GetMediaSize());
		
		Debug("-RTMPAACPacketizer::AddFrame() | Skip AACSequenceHeader frame\n");
		return nullptr;
	}
	
	if (audioFrame->GetAACPacketType()!=RTMPAudioFrame::AACRaw)
	{
		//DOne
		Debug("-RTMPAACPacketizer::AddFrame() | Skp AACRaw frame\n");
		return nullptr;
	}

	//IF we have aac config
	if (!gotConfig)
	{
		//Error
		Debug("-RTMPAACPacketizer::AddFrame() | Got AAC frame but not valid description yet\n");
		//DOne
		return nullptr;
	}

	if (!aacSpecificConfig.GetRate())
	{
		//Error
		Debug("-RTMPAACPacketizer::AddFrame() | Got zero rate, skipping packet\n");
		//DOne
		return nullptr;
	}

	if (!aacSpecificConfig.GetFrameLength())
	{
		//Error
		Debug("-RTMPAACPacketizer::AddFrame() | Got zero frame length, skipping packet\n");
		//DOne
		return nullptr;
	}


	//Create frame
	auto frame = std::make_unique<AudioFrame>(AudioCodec::AAC);
	
	//Set time in ms
	frame->SetTime(audioFrame->GetTimestamp());
	
	//Calculate timestamp in sample rate clock
	uint64_t timestamp = audioFrame->GetTimestamp() * aacSpecificConfig.GetRate() / 1000;

	//Round up to full frame duration
	timestamp = std::round<uint64_t>((double)timestamp / aacSpecificConfig.GetFrameLength() * aacSpecificConfig.GetFrameLength());

	//Set info from AAC config
	frame->SetClockRate(aacSpecificConfig.GetRate());
	frame->SetNumChannels(aacSpecificConfig.GetChannels());
	frame->SetTimestamp(timestamp);
	frame->SetDuration(aacSpecificConfig.GetFrameLength());

	//Allocate data
	frame->AllocateCodecConfig(aacSpecificConfig.GetSize());
	//Serialize it
	aacSpecificConfig.Serialize(frame->GetCodecConfigData(),frame->GetCodecConfigSize());
	
	//Add aac frame in single rtp 
	auto ini = frame->AppendMedia(audioFrame->GetMediaData(),audioFrame->GetMediaSize());
	frame->AddRtpPacket(ini,audioFrame->GetMediaSize(),nullptr,0);

	//DOne
	return frame;
}
