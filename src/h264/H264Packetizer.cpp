#include "H264Packetizer.h"

#include "video.h"
#include "h264/h264.h"

constexpr int FrameRepeatCount = 20;

H264Packetizer::H264Packetizer() : H26xPacketizer(VideoCodec::H264)
{
	// To account for potential packet loss, we repeat the SEI containing 
	// scte tags for 20 frames to provide added resiliency
	scteFrameRepeatCount = FrameRepeatCount;
}

void H264Packetizer::EmitNal(VideoFrame& frame, BufferReader nal)
{
	uint8_t naluHeader = nal.Peek1();
	uint8_t nri = naluHeader & 0b0'11'00000;
	uint8_t nalUnitType = naluHeader & 0b0'00'11111;

	std::string fuPrefix = { (char)(nri | 28u), (char)nalUnitType };
	H26xPacketizer::EmitNal(frame, nal, fuPrefix, 1);
}

void H264Packetizer::OnNal(VideoFrame& frame, BufferReader& reader, std::optional<bool>& frameEnd)
{
	//Return if current NAL is empty
	if (!reader.GetLeft())
		return;

	auto nalSize = reader.GetLeft();
	auto nalUnit = reader.PeekData();

	/* +---------------+
	 * |0|1|2|3|4|5|6|7|
	 * +-+-+-+-+-+-+-+-+
	 * |F|NRI|  Type   |
	 * +---------------+
	 */
	uint8_t naluHeader 	= reader.Get1();
	[[maybe_unused]] uint8_t nri		= naluHeader & 0b0'11'00000;
	uint8_t nalUnitType	= naluHeader & 0b0'00'11111;

	//UltraDebug("-SRTConnection::OnVideoData() | Got nal [type:%d,size:%d]\n", nalUnitType, nalSize);

	//Check if IDR/SPS/PPS, set Intra
	if ((nalUnitType == 0x05)
		|| (nalUnitType == 0x07)
		|| (nalUnitType == 0x08))
	{
		//We consider frames having an SPS/PPS as intra due to intra frame refresh
		frame.SetIntra(true);
	}

	//Check if IDR SPS or PPS
	switch (nalUnitType)
	{
		case 0x07: // SPS
		{
			//Check nal data size
			if (nalSize < 4)
				break;

			const uint8_t* nalData = reader.PeekData();

			//Set config
			config.SetConfigurationVersion(1);
			config.SetAVCProfileIndication(nalData[0]);
			config.SetProfileCompatibility(nalData[1]);
			config.SetAVCLevelIndication(nalData[2]);
			config.SetNALUnitLengthSizeMinus1(OUT_NALU_LENGTH_SIZE - 1);

			//Reset previous SPS only on the 1st SPS in current frame
			if (noSPSInFrame)
			{
				//UltraDebug("-SRTConnection::OnVideoData() | Clear cached SPS\n");
				config.ClearSequenceParameterSets();
				noSPSInFrame = false;
			}

			//Add full nal to config
			config.AddSequenceParameterSet(nalUnit, nalSize);

			//Parse sps
			{
				auto localSps = std::make_unique<H264SeqParameterSet>();
				if (localSps->Decode(nalData, nalSize - 1))
				{
					//Set dimensions
					frame.SetWidth(localSps->GetWidth());
					frame.SetHeight(localSps->GetHeight());
					
					sps = std::move(localSps);
				}
			}
			return;
		}
		case 0x08: // PPS
			//Reset previous PPS only on the 1st PPS in current frame
			if (noPPSInFrame)
			{
				//UltraDebug("-SRTConnection::OnVideoData() | Clear cached PPS\n");
				config.ClearPictureParameterSets();
				noPPSInFrame = false;
			}

			//Add full nal to config
			config.AddPictureParameterSet(nalUnit, nalSize);
			return;
		case 0x09:
			//Ignore fame acces unit delimiters
			return;
		case 0x01:
		case 0x02:
		case 0x05:
			if (sps)
			{
				H264SliceHeader header;
				if (header.Decode(reader.PeekData(), reader.GetLeft(), *sps))
				{
					// If no field pic flag, regard it as frame End
					frameEnd = header.GetFieldPicFlag() ? header.GetBottomFieldFlag() : true;

					auto sliceType = header.GetSliceType();
					frame.SetBFrame(sliceType == 1 || sliceType == 6);
				}
			}
			
			break;
	}

	// If this frame has been marked as intra (meaning it's either an IDR or a non-IDR
	// with SPS/PPS), and the configuration descriptor is complete, then make sure to
	// attach the configuration descriptor to the VideoFrame (for recording) and to
	// emit the parameter sets.
	//
	// We only need to do this once per frame, and the appropriate time to do it is
	// just before the first slice of the frame:
	if (
		// this NALU is a slice
		((nalUnitType >= 1 && nalUnitType <= 5) || (nalUnitType >= 19 && nalUnitType <= 21)) &&
		// it belongs to an intra frame
		frame.IsIntra() &&
		// no need to do this more than once per frame
		!frame.HasCodecConfig() &&
		// the configuration descriptor is fully populated by now
		config.GetNumOfPictureParameterSets() && config.GetNumOfSequenceParameterSets()
	)
	{
		const uint8_t spsNum = config.GetNumOfSequenceParameterSets();
		// Get total sps size
		// Can improve by adding AVCDescriptorconfig::GetSPSTotolSize() in media-server
		uint64_t spsTotalSize = 0;
		for (uint8_t i = 0; i < spsNum; i++)
			spsTotalSize += config.GetSequenceParameterSetSize(i);

		const uint8_t ppsNum = config.GetNumOfPictureParameterSets();
		// Get total pps size
		// Can improve by adding AVCDescriptorconfig::GetPPSTotolSize() in media-server
		uint64_t ppsTotalSize = 0;
		for (uint8_t i = 0; i < ppsNum; i++)
			ppsTotalSize += config.GetPictureParameterSetSize(i);

		//UltraDebug("-SRTConnection::OnVideoData() | SPS [num: %d, size: %d], PPS [num: %d, size: %d]\n", spsNum, spsTotalSize, ppsNum, ppsTotalSize);
		//Allocate enought space to prevent further reallocations
		// preffix num: spsNum + ps_num + 1
		frame.Alloc(frame.GetLength() + nalSize + spsTotalSize + ppsTotalSize + OUT_NALU_LENGTH_SIZE * (spsNum + ppsNum + 1));

		// Add SPS
		for (uint8_t i = 0; i < spsNum; i++)
			EmitNal(frame, BufferReader(config.GetSequenceParameterSet(i), config.GetSequenceParameterSetSize(i)));

		// Add PPS
		for (uint8_t i = 0; i < ppsNum; i++)
			EmitNal(frame, BufferReader(config.GetPictureParameterSet(i), config.GetPictureParameterSetSize(i)));

		//Serialize config in to frame
		frame.AllocateCodecConfig(config.GetSize());
		config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
	}
	
	if (scteMessage.GetSize() != 0)
	{	
		//Add unregistered SEI message NAL
		std::vector<uint8_t> sei;
		sei.push_back(0x06);	// SEI NAL
		sei.push_back(0x05);	// User unregistered data
		
		uint8_t uuid[16] = {
			0x2b, 0x69, 0xba, 0x1b, 0x77, 0x7e, 0x46, 0x53,
			0x99, 0x7a, 0xc6, 0x75, 0xb9, 0xd3, 0xac, 0xaf
		};
		sei.insert(sei.end(), uuid, uuid + sizeof(uuid));
		
		/* Construct json encoded as a string
		Example:
		{
  			id: 14194058,
  			start: 1717005742629,
  			duration: 30,
	  		tag: '/DBcAAAAAAAAAP/wBQb//ciI8QBGAh1DVUVJXQk9EX+fAQ5FUDAxODAzODQwMDY2NiEEZAIZQ1VFSV0JPRF/3wABLit7AQVDMTQ2NDABAQEKQ1VFSQCAMTUwKnPhdcU='
		}
		 We need to compute a unique hash for every scte payload received. 
		 The operation is idempotent so repeating tags will re use the same hash ID.
		*/
		uint32_t hashId = computeUniqueHash(reinterpret_cast<const char*>(scteMessage.GetData()), scteMessage.GetSize());

		std::string seiScteJson = "{\"id\":" 
								+ std::to_string(hashId)
								+ ",\"start\":" 
								+ std::to_string(scteTimestamp) 
								+ "," 
								+ "\"duration\":0,\"tag\":\"";
		
		// Convert scte data to base64
		uint8_t scteBase64[scteMessage.GetSize()*2] = {0};
		av_base64_encode((char*)scteBase64, scteMessage.GetSize()*2, scteMessage.GetData(), scteMessage.GetSize());
		uint32_t base64Len = 0;
		
		for (uint32_t i = 0; i < scteMessage.GetSize() * 2; i++)
		{
			if (scteBase64[i] == '\0')
			{
				base64Len = i;
				break;
			}
		}
		printf("\nLen is %u\n", base64Len);
		seiScteJson.append((char *) scteBase64, base64Len);
		seiScteJson += "\"}";
		const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&seiScteJson[0]);

		// Insert the payload size at pos 2 (zero indexed) 
		// [0] 0x06 SEI NAL 
		// [1] 0x05 Unregistered user data
		// [2] Payload size
		sei.insert(sei.begin() + 2, 16 + seiScteJson.length()); // UUID 16 bytes + Serialized JSON length

		// scte payload
		sei.insert(sei.end(), seiScteJson.begin(), seiScteJson.end());
		
		// rbsp_trailing_bits
		sei.push_back(0x80);

		//Escape nal
		uint8_t seiEscaped[sei.size()*2];
		auto seiSize = NalEscapeRbsp(seiEscaped, sizeof(seiEscaped), sei.data(), sei.size()).value();
		
		Debug("H264Packetizer::OnNal() | Emit SCTE data as SEI Unregistered User Data, Size %lu \n", seiSize);

		EmitNal(frame, BufferReader(seiEscaped, seiSize));
		if (scteFrameRepeatCount-- <= 0)
		{
			scteMessage.Reset();
			scteFrameRepeatCount = 20;
		}
		
	}
	

	EmitNal(frame, BufferReader(nalUnit, nalSize));
}

std::unique_ptr<MediaFrame> H264Packetizer::ProcessAU(BufferReader& reader)
{
	noPPSInFrame = true;
	noSPSInFrame = true;

	return H26xPacketizer::ProcessAU(reader);
}

void H264Packetizer::SetScteData(Buffer data)
{
	scteMessage = std::move(data);
}

std::optional<Buffer> H264Packetizer::GetScteData()
{
	if (scteMessage.GetData() == nullptr) return std::nullopt;
	
	auto buffer = std::move(scteMessage);
	
	return buffer;
}

void H264Packetizer::SetScteTimestamp(uint64_t time)
{
	scteTimestamp = time;
}

uint64_t H264Packetizer::GetScteTimestamp()
{
	return scteTimestamp;
}