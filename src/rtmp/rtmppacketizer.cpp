
#include "rtmp/rtmppacketizer.h"
#include "av1/AV1.h"
#include "av1/Obu.h"
#include "h264/H26xNal.h"


VideoCodec::Type GetRtmpFrameVideoCodec(const RTMPVideoFrame& videoFrame)
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
		case RTMPVideoFrame::H264:
			return VideoCodec::H264;

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

AudioCodec::Type GetRtmpFrameAudioCodec(const RTMPAudioFrame& audioFrame)
{
	switch (audioFrame.GetAudioCodec())
	{
		case RTMPAudioFrame::AAC:
			return AudioCodec::AAC;
		case RTMPAudioFrame::G711A:
			return AudioCodec::PCMA;
		case RTMPAudioFrame::G711U:
			return AudioCodec::PCMU;
		default:
			return AudioCodec::UNKNOWN;
	}
}

std::unique_ptr<RTMPVideoPacketizer> CreateRTMPVideoPacketizer(VideoCodec::Type type)
{
	switch (type)
	{
		case VideoCodec::H264:
			return std::unique_ptr<RTMPVideoPacketizer>(new RTMPAVCPacketizer());
		case VideoCodec::H265:
			return std::unique_ptr<RTMPVideoPacketizer>(new RTMPHEVCPacketizer());
		case VideoCodec::AV1:
			return std::unique_ptr<RTMPVideoPacketizer>(new RTMPAv1Packetizer());
		default:
			return nullptr;
	}
}

std::unique_ptr<RTMPAudioPacketizer> CreateRTMPAudioPacketizer(AudioCodec::Type type)
{
	switch (type)
	{
		case AudioCodec::AAC:
			return std::unique_ptr<RTMPAudioPacketizer>(new RTMPAACPacketizer());
		case AudioCodec::PCMA:
			return std::unique_ptr<RTMPAudioPacketizer>(new RTMPG711APacketizer());
		case AudioCodec::PCMU:
			return std::unique_ptr<RTMPAudioPacketizer>(new RTMPG711UPacketizer());
		default:
			return nullptr;
	}
}

template<typename DescClass, VideoCodec::Type codec>
std::unique_ptr<VideoFrame> RTMPVideoPacketizerImpl<DescClass, codec>::PrepareFrame(RTMPVideoFrame* videoFrame)
{
	//UltraDebug("-RTMPPacketizer::PrepareFrame() [codec:%d, isConfig:%d, isCodedFrames:%d]\n", GetRtmpFrameVideoCodec(*videoFrame), videoFrame->IsConfig(), videoFrame->IsCodedFrames());
	
	//Check it is processing codec
	if (GetRtmpFrameVideoCodec(*videoFrame) != codec)
		//Ignore
		return nullptr;
	
	//Check if it is descriptor
	if (videoFrame->IsConfig())
	{
		//Parse it
		if(desc.Parse(videoFrame->GetMediaData(),videoFrame->GetMediaSize())) 
		{
			//Got config
			gotConfig = true;
		} else {
			//Show error
			Warning(" RTMPPacketizer::PrepareFrame() | Config parse error\n");
			//Dump data
			videoFrame->Dump();
			::Dump(videoFrame->GetMediaData(), videoFrame->GetMediaSize());
		}
		//DOne
		return nullptr;
	}
	
	//It should be a nalu then
	if (!videoFrame->IsCodedFrames())
	{
		//Error
		Warning("-RTMPPacketizer::PrepareFrame() | Not a coded frame\n");
		//DOne
		return nullptr;
	}
	
	//Ensure that we have got config
	if (!gotConfig)
	{
		//Error
		Warning("-RTMPPacketizer::PrepareFrame() | Got media frame but not valid description yet\n");
		//DOne
		return nullptr;
	}
	
	//Create frame
	auto frame = std::make_unique<VideoFrame>(codec,videoFrame->GetSize()+desc.GetSize()+256);

	//Set intra flag
	frame->SetIntra(videoFrame->GetFrameType() == RTMPVideoFrame::INTRA);
	
	//Set time
	frame->SetTime(videoFrame->GetTimestamp());
	//Set clock rate
	frame->SetClockRate(1000);
	//Set timestamp
	frame->SetTimestamp(videoFrame->GetTimestamp());
	frame->SetPresentationTimestamp(videoFrame->GetTimestamp() + videoFrame->GetCTS());


	//Set Sender time
	frame->SetSenderTime(videoFrame->GetSenderTime());
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

	return frame;
}

template<typename DescClass, typename SPSClass, typename PPSClass, VideoCodec::Type codec>
std::unique_ptr<VideoFrame> RTMPH26xPacketizer<DescClass, SPSClass, PPSClass, codec>::AddFrame(RTMPVideoFrame* videoFrame)
{
	//Debug("-RTMPH26xPacketizer::AddFrame() [size:%u,intra:%d]\n",videoFrame->GetMediaSize(), videoFrame->GetFrameType() == RTMPVideoFrame::INTRA);
	
	auto frame = this->PrepareFrame(videoFrame);
	if (!frame) return nullptr;	
	
	auto& desc = this->desc;
	
	//GEt nal header length
	DWORD nalUnitLength = desc.GetNALUnitLengthSizeMinus1() + 1;
	//Create it
	BYTE nalHeader[4];
		
	//If is an intra
	if (videoFrame->GetFrameType()==RTMPVideoFrame::INTRA)
	{
		if constexpr (std::is_same_v<DescClass, HEVCDescriptor>)
		{
			//Add VPS
			for (int i=0;i<desc.GetNumOfVideoParameterSets();i++)
			{
				//Set size
				setN(nalUnitLength, nalHeader, 0, desc.GetVideoParameterSetSize(i));
				
				//Append nal size header
				frame->AppendMedia(nalHeader, nalUnitLength);
				
				//Append nal
				auto ini = frame->AppendMedia(desc.GetVideoParameterSet(i),desc.GetVideoParameterSetSize(i));
				
				//Crete rtp packet
				frame->AddRtpPacket(ini,desc.GetVideoParameterSetSize(i),nullptr,0);
			}
		}
		
		//Decode SPS
		for (int i=0;i<desc.GetNumOfSequenceParameterSets();i++)
		{
			//Set size
			setN(nalUnitLength, nalHeader, 0, desc.GetSequenceParameterSetSize(i));
			
			//Append nal size header
			frame->AppendMedia(nalHeader, nalUnitLength);
			
			//Append nal
			auto ini = frame->AppendMedia(desc.GetSequenceParameterSet(i),desc.GetSequenceParameterSetSize(i));
			
			//Crete rtp packet
			frame->AddRtpPacket(ini,desc.GetSequenceParameterSetSize(i),nullptr,0);
			
			//Parse sps skipping nal header
			auto nalHeaderSize = (codec == VideoCodec::H264) ? 1 : 2;
			SPSClass localSps;
			if (localSps.Decode(desc.GetSequenceParameterSet(i)+nalHeaderSize,desc.GetSequenceParameterSetSize(i)-nalHeaderSize))
			{
				sps = localSps;
			}
		}
		//Decode PPS
		for (int i=0;i<desc.GetNumOfPictureParameterSets();i++)
		{
			//Set size
			setN(nalUnitLength, nalHeader, 0, desc.GetPictureParameterSetSize(i));
			
			//Append nal size header
			frame->AppendMedia(nalHeader, nalUnitLength);
			//Append nal
			auto ini = frame->AppendMedia(desc.GetPictureParameterSet(i),desc.GetPictureParameterSetSize(i));
			
			//Crete rtp packet
			frame->AddRtpPacket(ini,desc.GetPictureParameterSetSize(i),nullptr,0);
			
			//Parse sps skipping nal header
			auto nalHeaderSize = (codec == VideoCodec::H264) ? 1 : 2;
			PPSClass localPps;
			if (localPps.Decode(desc.GetPictureParameterSet(i)+nalHeaderSize,desc.GetPictureParameterSetSize(i)-nalHeaderSize))
			{
				pps = localPps;
			}
		}
	}
	
	if (sps.has_value())
	{
		//Set dimensions
		frame->SetWidth(sps->GetWidth());
		frame->SetHeight(sps->GetHeight());
	}

	if (videoFrame->GetSenderTime())
	{
		               
		//Add unregistered SEI message NAL
		uint8_t sei[28] = { 0x06, 0x05, 0x18, 0x9a, 0x21, 0xf3, 0xbe, 0x31,
				    0xf0, 0x4b, 0x78, 0xb0, 0xbe, 0xc7, 0xf7, 0xdb,
				    0xb9, 0x72, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0x00, 0x80 };

		//Set timestamp
		set8(sei, 19, videoFrame->GetSenderTime());

		//Escape nal
		uint8_t seiEscaped[sizeof(sei)*2];
		auto seiSize = NalEscapeRbsp(seiEscaped, sizeof(seiEscaped), sei, sizeof(sei)).value();

		//Set size after escaping
		setN(nalUnitLength, nalHeader, 0, seiSize);

		//Append nal size header
		frame->AppendMedia(nalHeader, nalUnitLength);

		//Append nal
		auto ini = frame->AppendMedia(seiEscaped, seiSize);

		//Crete rtp packet
		frame->AddRtpPacket(ini, seiSize, nullptr, 0);
	}

	//Malloc
	const BYTE *data = videoFrame->GetMediaData();
	//Get size
	DWORD size = videoFrame->GetMediaSize();
	
	bool parsingFrameType = true;
	
	//Chop into NALs
	while(size>nalUnitLength)
	{
		//Get size
		DWORD nalSize = getN(nalUnitLength, data, 0);
		
		//Ensure we have enougth data
		if (nalSize+nalUnitLength>size)
		{
			//Error
			Warning("-RTMPAVCPacketizer::AddFrame() Error adding size=%d nalSize=%d fameSize=%d\n",size,nalSize,videoFrame->GetMediaSize());
			//Skip
			break;
		}

		//Get NAL start
		const BYTE* nal = data + nalUnitLength;

		//Skip fill data nalus for h264
		if (codec == VideoCodec::H264 && nal[0] == 12)
		{
			//Skip it
			data += nalUnitLength + nalSize;
			size -= nalUnitLength + nalSize;
			//Next
			continue;
		}
		
		if (parsingFrameType && sps.has_value() && pps.has_value())
		{
			if constexpr (std::is_same_v<SPSClass, H264SeqParameterSet>)
			{
				const BYTE nalUnitType = nal[0] & 0x1f;
				if (nalUnitType == 1 || nalUnitType == 2 || nalUnitType == 5)
				{
					H264SliceHeader header;
					if (header.Decode(nal + 1, nalSize - 1, *sps))
					{
						auto sliceType = header.GetSliceType();
						frame->SetBFrame(sliceType == 1 || sliceType == 6);
						
						parsingFrameType = false;
					}
				}
			}
			else if constexpr (std::is_same_v<SPSClass, H265SeqParameterSet>)
			{
				auto naluHeader = get2(nal, 0);
				BYTE nalUnitType = (naluHeader >> 9) & 0b111111;
				if ((nalUnitType <= HEVC_RTP_NALU_Type::RASL_R) || 
					(nalUnitType >= HEVC_RTP_NALU_Type::BLA_W_LP && nalUnitType <= HEVC_RTP_NALU_Type::CRA_NUT))
				{
					H265SliceHeader header;
					if (header.Decode(nal + 2, nalSize - 2, nalUnitType, *pps, *sps))
					{
						frame->SetBFrame(header.GetSliceType() == 0);
						
						parsingFrameType = false;
					}
				}
			}
		}

		//Append nal header
		frame->AppendMedia(data, nalUnitLength);

		//Log("-NAL %x size=%d nalSize=%d fameSize=%d\n",nal[0],size,nalSize,videoFrame->GetMediaSize());
		
		//Append nal
		auto ini = frame->AppendMedia(nal,nalSize);
		
		//Skip it
		data+=nalUnitLength+nalSize;
		size-=nalUnitLength+nalSize;
		
		//Check if NAL is bigger than RTPPAYLOADSIZE
		if (nalSize>RTPPAYLOADSIZE)
		{
			std::vector<uint8_t> fragmentationHeader;
			if (codec == VideoCodec::H264)
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
				fragmentationHeader = {
					(BYTE)((nal[0] & 0xE0) | 28),
					(BYTE)(0x80 | nalUnitType)
				};
				
				//Skip original header
				ini++;
				nalSize--;
			}
			else
			{
				auto naluHeader = get2(nal, 0);
				BYTE nalUnitType		= (naluHeader >> 9) & 0b111111;
				BYTE nuh_layer_id		= (naluHeader >> 3) & 0b111111;
				BYTE nuh_temporal_id_plus1	= naluHeader & 0b111;

				const uint16_t nalHeaderFU = ((uint16_t)(HEVC_RTP_NALU_Type::UNSPEC49_FU) << 9)
									| ((uint16_t)(nuh_layer_id) << 3)
									| ((uint16_t)(nuh_temporal_id_plus1)); 
				fragmentationHeader.resize(3);
				set2(fragmentationHeader.data(), 0, nalHeaderFU);
				fragmentationHeader[2] = nalUnitType | 0x80;
				
				//Skip original header
				ini+= 2;
				nalSize-=2;
			}

			//Add all
			while (nalSize)
			{
				//Get fragment size
				auto fragSize = std::min(nalSize,(DWORD)(RTPPAYLOADSIZE-fragmentationHeader.size()));
				//Check if it is last
				if (fragSize==nalSize)
					//Set end bit
					fragmentationHeader.back() |= 0x40;
				//Add rpt packet info
				frame->AddRtpPacket(ini,fragSize,fragmentationHeader.data(),fragmentationHeader.size());
				//Remove start bit
				fragmentationHeader.back() &= 0x7F;
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


template class RTMPH26xPacketizer<AVCDescriptor, H264SeqParameterSet, H264PictureParameterSet, VideoCodec::H264>;
template class RTMPH26xPacketizer<HEVCDescriptor, H265SeqParameterSet, H265PictureParameterSet, VideoCodec::H265>;


std::unique_ptr<VideoFrame> RTMPAv1Packetizer::AddFrame(RTMPVideoFrame* videoFrame)
{
	//UltraDebug("-RTMPAv1Packetizer::AddFrame() [size:%u,intra:%d]\n",videoFrame->GetMediaSize(), videoFrame->GetFrameType() == RTMPVideoFrame::INTRA);
	
	auto frame = PrepareFrame(videoFrame);
	if (!frame) return nullptr;
		
	ObuHeader obuHeader;
	RtpAv1AggreationHeader header;

	//If is an intra
	if (videoFrame->GetFrameType()==RTMPVideoFrame::INTRA)
	{
		//TODO: We are probably duplicating the sequence header here
		if (desc.sequenceHeader)
		{
			auto ini = frame->AppendMedia(*desc.sequenceHeader);
			BufferReader reader(*desc.sequenceHeader);

			if (obuHeader.Parse(reader))
			{
				//Get length from header or read the rest available
				auto payloadSize = obuHeader.length.value_or(reader.GetLeft());
				
				//Skip headers
				ini += reader.Mark();

				//Debug("-RTMPAv1Packetizer::AddFrame() desc [left:%llu,payloadSize:%llu, pos:%llu]\n", reader.GetLeft(), payloadSize, ini);

				SequenceHeaderObu sho;
				if (sho.Parse(reader))
				{
					//Set dimensions
					frame->SetWidth(sho.max_frame_width_minus_1 + 1);
					frame->SetHeight(sho.max_frame_height_minus_1 + 1);
				}

				//RTP aggregation header for only 1 OBU segment
				header.field.W = 1; // Send one OBU element each time
				header.field.Z = 0; // No prev fragment
				header.field.Y = 0; // No next fragment
				header.field.N = frame->GetRtpPacketizationInfo().empty() && frame->IsIntra() ? 1 : 0; // First packet of new coded sequence

				//RTP preffix
				Buffer preffix(header.GetSize() + obuHeader.GetSize());
				BufferWritter writter(preffix);

				//Write fragmentation header 
				header.Serialize(writter);
				//Remove length
				obuHeader.length.reset();
				//Write OBU header
				obuHeader.Serialize(writter);
				//Add rtp packet
				frame->AddRtpPacket(ini, payloadSize, writter.GetData(), writter.GetLength());
			}
		}
		

	}

	//Get media data and size
	const BYTE *data = videoFrame->GetMediaData();
	DWORD size = videoFrame->GetMediaSize();

	//Copy data to frame
	auto ini = frame->AppendMedia(data, size);

	//Get reader for av1 encoded packet
	BufferReader reader(data, size);

	//Get initial position of reader
	auto mark = reader.Mark();
	
	//For each obu
	while (reader.GetLeft() && obuHeader.Parse(reader))
	{
		//Get length from header or read the rest available
		auto payloadSize = obuHeader.length.value_or(reader.GetLeft());
		//Skip header and size from media data
		auto pos = ini + reader.GetOffset(mark);

		//UltraDebug("-RTMPAv1Packetizer::AddFrame() [left:%llu,payloadSize:%llu, pos:%llu]\n", reader.GetLeft(), payloadSize, pos);

		//Ensure we have enought data for the rest of the obu
		if (!reader.Assert(payloadSize))
			break;

		//Skip the rest of the obu
		reader.Skip(payloadSize);

		//We are not going to to write the length of the obu framgent
		obuHeader.length.reset();

		//If we need to fragmentize into multiple rtp packets
		if ((payloadSize + header.GetSize() + obuHeader.GetSize()) > RTPPAYLOADSIZE)
		{
			bool firstSegment = true;

			while (payloadSize)
			{
				//Calculate fragment size
				auto fragSize = std::min(payloadSize, size_t(RTPPAYLOADSIZE - header.GetSize() - obuHeader.GetSize()));

				//RTP aggregation header for only 1 OBU segment
				header.field.W = 1;										 // Send one OBU element each time
				header.field.Z = !firstSegment;							 // Has prev fragment
				header.field.Y = (fragSize == payloadSize) ? 0 : 1;		 // Has next fragment
				header.field.N = frame->GetRtpPacketizationInfo().empty() && frame->IsIntra() ? 1 : 0; // First packet of new coded sequence

				//RTP preffix with max possible size
				Buffer preffix(header.GetSize() + obuHeader.GetSize());
				BufferWritter writter(preffix);

				//Write fragmentation header 
				header.Serialize(writter);

				if (firstSegment)
				{
					//Write OBU header on first segment
					obuHeader.Serialize(writter);
					//Next is not first
					firstSegment = false;
				}

				//Add rtp packet
				frame->AddRtpPacket(pos, fragSize, writter.GetData(), writter.GetLength());
				//Next fragment
				pos += fragSize;
				//Remove fragment size from total
				payloadSize -= fragSize;
			}
		}
		else
		{
			//RTP aggregation header for only 1 OBU segment
			header.field.W = 1; // Send one OBU element each time
			header.field.Z = 0; // No prev fragment
			header.field.Y = 0; // No next fragment
			header.field.N = frame->GetRtpPacketizationInfo().empty() && frame->IsIntra() ? 1 : 0; // First packet of new coded sequence

			//RTP preffix
			Buffer preffix(header.GetSize() + obuHeader.GetSize());
			BufferWritter writter(preffix);

			//Write fragmentation header 
			header.Serialize(writter);
			//Write OBU header
			obuHeader.Serialize(writter);
			//Add rtp packet
			frame->AddRtpPacket(pos, payloadSize, writter.GetData(), writter.GetLength());
		}
	}

	//Done
	return frame;
}


std::unique_ptr<AudioFrame> RTMPAACPacketizer::AddFrame(RTMPAudioFrame* audioFrame)
{
	//UltraDebug("-RTMPAACPacketizer::AddFrame() [size:%u,aac:%d,codec:%d]\n",audioFrame->GetMediaSize(),audioFrame->GetAACPacketType(),audioFrame->GetAudioCodec());
	
	if (audioFrame->GetAudioCodec()!=RTMPAudioFrame::AAC)
	{
		Debug("-RTMPAACPacketizer::AddFrame() | Skiping non-AAC codec\n");
		return nullptr;
	}
	
	//Check if it is AAC descriptor
	if (audioFrame->GetAACPacketType()==RTMPAudioFrame::AACSequenceHeader)
	{
		//Handle specific settings
		gotConfig = aacSpecificConfig.Decode(audioFrame->GetMediaData(),audioFrame->GetMediaSize());
		
		Debug("-RTMPAACPacketizer::AddFrame() | Got AACSequenceHeader frame\n");
		aacSpecificConfig.Dump();
		return nullptr;
	}
	
	if (audioFrame->GetAACPacketType()!=RTMPAudioFrame::AACRaw)
	{
		//DOne
		Debug("-RTMPAACPacketizer::AddFrame() | Skiping non AACRaw frame\n");
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


std::unique_ptr<AudioFrame> RTMPG711APacketizer::AddFrame(RTMPAudioFrame* audioFrame)
{
	//UltraDebug("-RTMPG711APacketizer::AddFrame() [size:%u,aac:%d,codec:%d]\n",audioFrame->GetMediaSize(),audioFrame->GetAACPacketType(),audioFrame->GetAudioCodec());

	//Create frame
	auto frame = std::make_unique<AudioFrame>(AudioCodec::PCMA);

	//Set time in ms
	frame->SetTime(audioFrame->GetTimestamp());

	//Calculate timestamp in sample rate clock
	uint64_t timestamp = audioFrame->GetTimestamp() *8;

	//Set info from AAC config
	frame->SetClockRate(8000);
	frame->SetNumChannels(1);
	frame->SetTimestamp(timestamp);
	frame->SetDuration(160);
	
	//Add frame in single rtp 
	auto ini = frame->AppendMedia(audioFrame->GetMediaData(), audioFrame->GetMediaSize());
	frame->AddRtpPacket(ini, audioFrame->GetMediaSize(), nullptr, 0);

	//DOne
	return frame;
}

std::unique_ptr<AudioFrame> RTMPG711UPacketizer::AddFrame(RTMPAudioFrame* audioFrame)
{
	//UltraDebug("-RTMPG711UPacketizer::AddFrame() [size:%u,aac:%d,codec:%d]\n",audioFrame->GetMediaSize(),audioFrame->GetAACPacketType(),audioFrame->GetAudioCodec());

	//Create frame
	auto frame = std::make_unique<AudioFrame>(AudioCodec::PCMU);

	//Set time in ms
	frame->SetTime(audioFrame->GetTimestamp());

	//Calculate timestamp in sample rate clock
	uint64_t timestamp = audioFrame->GetTimestamp() * 8;

	//Set info from AAC config
	frame->SetClockRate(8000);
	frame->SetNumChannels(1);
	frame->SetTimestamp(timestamp);
	frame->SetDuration(160);

	//Add frame in single rtp 
	auto ini = frame->AppendMedia(audioFrame->GetMediaData(), audioFrame->GetMediaSize());
	frame->AddRtpPacket(ini, audioFrame->GetMediaSize(), nullptr, 0);

	//DOne
	return frame;
}
