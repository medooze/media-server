
#include "rtmp/rtmppacketizer.h"

const BYTE AnnexBStartCode[4] = {0x00,0x00,0x00,0x01};

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
		if(!desc.Parse(videoFrame->GetMediaData(),videoFrame->GetMaxMediaSize()))
			//Show error
			Error("AVCDescriptor parse error\n");
		//DOne
		return nullptr;
	}
	
	//It should be a nalu then
	if (videoFrame->GetAVCType()!=RTMPVideoFrame::AVCNALU)
		//DOne
		return nullptr;
	
	//Create frame
	auto frame = std::make_unique<VideoFrame>(VideoCodec::H264,videoFrame->GetSize()+desc.GetSize()+256);
	
	//Set time
	frame->SetTimestamp(videoFrame->GetTimestamp());
		
	//If is an intra
	if (videoFrame->GetFrameType()==RTMPVideoFrame::INTRA)
	{
		//Decode SPS
		for (int i=0;i<desc.GetNumOfSequenceParameterSets();i++)
		{
			//Add annex b nal unit start code
			frame->AppendMedia((BYTE*)AnnexBStartCode,sizeof(AnnexBStartCode));
			//Append nal
			auto ini = frame->AppendMedia(desc.GetSequenceParameterSet(i),desc.GetSequenceParameterSetSize(i));
			//Send NAL
			frame->AddRtpPacket(ini,desc.GetSequenceParameterSetSize(i),nullptr,0);
		}
		//Decode PPS
		for (int i=0;i<desc.GetNumOfPictureParameterSets();i++)
		{
			//Add annex b nal unit start code
			frame->AppendMedia((BYTE*)AnnexBStartCode,sizeof(AnnexBStartCode));
			//Append nal
			auto ini = frame->AppendMedia(desc.GetPictureParameterSet(i),desc.GetPictureParameterSetSize(i));
			//Send NAL
			frame->AddRtpPacket(ini,desc.GetPictureParameterSetSize(i),nullptr,0);
		}
	}

	//GEt nal header length
	auto nalUnitLength = desc.GetNALUnitLength() + 1;
		
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
		//Get NAL start
		BYTE *nal = data+nalUnitLength;
		//Skip it
		data+=nalUnitLength+nalSize;
		size-=nalUnitLength+nalSize;
		
		//Add annex b nal unit start code
		frame->AppendMedia((BYTE*)AnnexBStartCode,sizeof(AnnexBStartCode));
		
		//Append nal
		auto ini = frame->AppendMedia(nal,nalSize);
		
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
			BYTE fragmentationHeader[2] = {(nal[0] & 0xE0) | 28 , 0x80 | nalUnitType};
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
		
		//Done
		return frame;
	}
}


std::unique_ptr<AudioFrame> RTMPAACPacketizer::AddFrame(RTMPAudioFrame* audioFrame)
{
	if (audioFrame->GetAudioCodec()!=RTMPAudioFrame::AAC)
		return nullptr;
	
	//Check if it is AAC descriptor
	if (audioFrame->GetAACPacketType()==RTMPAudioFrame::AACSequenceHeader)
		//TODO: Handle specific settings
		return nullptr;
	
	if (audioFrame->GetAACPacketType()!=RTMPAudioFrame::AACRaw)
		//DOne
		return nullptr;
	
	//Create frame
	auto frame = std::make_unique<AudioFrame>(AudioCodec::AAC,audioFrame->GetSize());
	
	//Add aac frame in single rtp 
	auto ini = frame->AppendMedia(audioFrame->GetMediaData(),audioFrame->GetMediaSize());
	frame->AddRtpPacket(ini,audioFrame->GetMediaSize(),nullptr,0);
	
	//DOne
	return frame;
	
}
