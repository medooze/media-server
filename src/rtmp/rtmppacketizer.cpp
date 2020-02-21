
#include "rtmp/rtmppacketizer.h"
#include "h264/h264.h"

std::unique_ptr<VideoFrame> RTMPAVCPacketizer::AddFrame(RTMPVideoFrame* videoFrame)
{
	//Debug("-RTMPAVCPacketizer::AddFrame() [size:%u]\n",videoFrame->GetMediaSize());
	
	//Check it is AVC
	if (videoFrame->GetVideoCodec()!=RTMPVideoFrame::AVC)
		//Ignore
		return nullptr;
		
	//Check if it is AVC descriptor
	if (videoFrame->GetAVCType()==RTMPVideoFrame::AVCHEADER)
	{
		//Parse it
		if(desc.Parse(videoFrame->GetMediaData(),videoFrame->GetMaxMediaSize()))
			//Got config
			gotConfig = true;
		else
			//Show error
			Error(" RTMPAVCPacketizer::AddFrame() | AVCDescriptor parse error\n");
		//DOne
		return nullptr;
	}
	
	//It should be a nalu then
	if (videoFrame->GetAVCType()!=RTMPVideoFrame::AVCNALU)
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
	
	//GEt nal header length
	DWORD nalUnitLength = desc.GetNALUnitLength() + 1;
	//Create it
	BYTE nalHeader[4];
	
	//Create frame
	auto frame = std::make_unique<VideoFrame>(VideoCodec::H264,videoFrame->GetSize()+desc.GetSize()+256);
	
	//Set clock rate
	frame->SetClockRate(1000);
	//Set time
	frame->SetTimestamp(videoFrame->GetTimestamp());
	
	//Get AVC data size
	auto configSize = desc.GetSize();
	//Allocate mem
	BYTE* config = (BYTE*)malloc(configSize);
	//Serialize AVC codec data
	DWORD configLen = desc.Serialize(config,configSize);
	//Set it
	frame->SetCodecConfig(config,configLen);
	//Free data
	free(config);
		
	//If is an intra
	if (videoFrame->GetFrameType()==RTMPVideoFrame::INTRA)
	{
		//Decode SPS
		for (int i=0;i<desc.GetNumOfSequenceParameterSets();i++)
		{
			//Set size
			set4(nalHeader,0,desc.GetSequenceParameterSetSize(i));
			
			//Append nal size header
			frame->AppendMedia(nalHeader, nalUnitLength);
			
			//Append nal
			auto ini =frame->AppendMedia(desc.GetSequenceParameterSet(i),desc.GetSequenceParameterSetSize(i));
			
			//Crete rtp packet
			frame->AddRtpPacket(ini,desc.GetSequenceParameterSetSize(i),nullptr,0);
			
			//Parse sps skipping nal type (first byte)
			H264SeqParameterSet sps;
			if (sps.Decode(desc.GetSequenceParameterSet(i)+1,desc.GetSequenceParameterSetSize(i)-1))
			{
				//Set dimensions
				frame->SetWidth(sps.GetWidth());
				frame->SetHeight(sps.GetHeight());
			}
		}
		//Decode PPS
		for (int i=0;i<desc.GetNumOfPictureParameterSets();i++)
		{
			//Set size
			set4(nalHeader,0,desc.GetPictureParameterSetSize(i));
			
			//Append nal size header
			frame->AppendMedia(nalHeader, nalUnitLength);
			//Append nal
			auto ini = frame->AppendMedia(desc.GetPictureParameterSet(i),desc.GetPictureParameterSetSize(i));
			
			//Crete rtp packet
			frame->AddRtpPacket(ini,desc.GetPictureParameterSetSize(i),nullptr,0);
		}
		//Set intra flag
		frame->SetIntra(true);
	}

	//Malloc
	BYTE *data = videoFrame->GetMediaData();
	//Get size
	DWORD size = videoFrame->GetMediaSize();
	
	//Chop into NALs
	while(size>nalUnitLength)
	{
		DWORD nalSize = 0;
		//Get size
		if (nalUnitLength==4)
			//Get size
			nalSize = get4(data,0);
		else if (nalUnitLength==3)
			//Get size
			nalUnitLength = get3(data,0);
		else if (nalUnitLength==2)
			//Get size
			nalSize = get2(data,0);
		else if (nalUnitLength==1)
			//Get size
			nalSize = data[0];
		else
			//Skip
			break;
		
		//Ensure we have enougth data
		if (nalSize+nalUnitLength>size)
		{
			//Error
			Error("-RTMPAVCPacketizer::AddFrame() Error adding size=%d nalSize=%d fameSize=%d\n",size,nalSize,videoFrame->GetMediaSize());
			//Skip
			break;
		}

		//Append nal header
		frame->AppendMedia(data, nalUnitLength);

		//Get NAL start
		BYTE *nal = data+nalUnitLength;
		
		//Skip fill data nalus
		if (nal[0]==12)
		{
			//Skip it
			data+=nalUnitLength+nalSize;
			size-=nalUnitLength+nalSize;
			//Next
			continue;
		}
		
		//Log("-NAL %x size=%d nalSize=%d fameSize=%d\n",nal[0],size,nalSize,videoFrame->GetMediaSize());
		
		//Append nal
		auto ini = frame->AppendMedia(nal,nalSize);
		
		//Skip it
		data+=nalUnitLength+nalSize;
		size-=nalUnitLength+nalSize;
		
		//Check if NAL is bigger than RTPPAYLOADSIZE
		if (nalSize>RTPPAYLOADSIZE)
		{
			//Get original nal type
			BYTE nalUnitType = nal[0] & 0x1f;
			//The fragmentation unit A header, S=1
			/* +---------------+---------------+
			 * |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
			 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 * |F|NRI|   28    |S|E|R| Type	   |
			 * +---------------+---------------+
			*/
			BYTE fragmentationHeader[2] = {
				(BYTE)((nal[0] & 0xE0) | 28),
				(BYTE)(0x80 | nalUnitType)
			};
			//Skip original header
			ini++;
			nalSize--;
			//Add all
			while (nalSize)
			{
				//Get fragment size
				auto fragSize = std::min(nalSize,(DWORD)(RTPPAYLOADSIZE-sizeof(fragmentationHeader)));
				//Check if it is last
				if (fragSize==nalSize)
					//Set end bit
					fragmentationHeader[1] |= 0x40;
				//Add rpt packet info
				frame->AddRtpPacket(ini,fragSize,fragmentationHeader,sizeof(fragmentationHeader));
				//Remove start bit
				fragmentationHeader[1] &= 0x7F;
				//Next
				ini += fragSize;
				//Remove size
				nalSize -= fragSize;
					
			}
		} else {
			//Add nal unit
			frame->AddRtpPacket(ini,nalSize,nullptr,0);
		}
	}
 		
	//Done
	return frame; 
}


std::unique_ptr<AudioFrame> RTMPAACPacketizer::AddFrame(RTMPAudioFrame* audioFrame)
{
	//Debug("-RTMPAACPacketizer::AddFrame() [size:%u,aac:%d,codec:%d]\n",audioFrame->GetMediaSize(),audioFrame->GetAACPacketType(),audioFrame->GetAudioCodec());
	
	if (audioFrame->GetAudioCodec()!=RTMPAudioFrame::AAC)
		return nullptr;
	
	//Check if it is AAC descriptor
	if (audioFrame->GetAACPacketType()==RTMPAudioFrame::AACSequenceHeader)
	{
		//Handle specific settings
		gotConfig = aacSpecificConfig.Decode(audioFrame->GetMediaData(),audioFrame->GetMediaSize());
		return nullptr;
	}
	
	if (audioFrame->GetAACPacketType()!=RTMPAudioFrame::AACRaw)
		//DOne
		return nullptr;
	
	//Create frame
	auto frame = std::make_unique<AudioFrame>(AudioCodec::AAC);
	
	//Set clock rate
	frame->SetClockRate(1000);
	//Set time
	frame->SetTimestamp(audioFrame->GetTimestamp());
	
	//IF we have aac config
	if (gotConfig)
	{
		//Allocate data
		frame->AllocateCodecConfig(aacSpecificConfig.GetSize());
		//Serialize it
		aacSpecificConfig.Serialize(frame->GetCodecConfigData(),frame->GetCodecConfigSize());
	}
	
	//Add aac frame in single rtp 
	auto ini = frame->AppendMedia(audioFrame->GetMediaData(),audioFrame->GetMediaSize());
	frame->AddRtpPacket(ini,audioFrame->GetMediaSize(),nullptr,0);
	
	//DOne
	return frame;
}
