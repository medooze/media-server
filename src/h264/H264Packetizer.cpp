#include "H264Packetizer.h"

#include "video.h"
#include "h264/h264.h"

H264Packetizer::H264Packetizer() : H26xPacketizer(VideoCodec::H264)
{
	sei.clear();            // Flush any previous data
	sei.push_back(0x06);	// SEI NAL
	sei.push_back(0x05);	// User unregistered data
	sei.push_back(0x00);    // Payload size
	
	uint8_t uuid[16] = {
		0x2b, 0x69, 0xba, 0x1b, 0x77, 0x7e, 0x46, 0x53,
		0x99, 0x7a, 0xc6, 0x75, 0xb9, 0xd3, 0xac, 0xaf
	};
	sei.insert(sei.end(), uuid, uuid + sizeof(uuid));
	scteMessageId = 0;
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
		//Escape nal
		uint8_t seiEscaped[sei.size()*2];
		auto seiSize = NalEscapeRbsp(seiEscaped, sizeof(seiEscaped), sei.data(), sei.size()).value();
		Debug("H264Packetizer::OnNal() | Emit SCTE data as SEI Unregistered User Data, Size %lu \n", seiSize);

		EmitNal(frame, BufferReader(seiEscaped, seiSize));
		if (repeatFrameCounter-- <= 0)
		{
			scteMessage.Reset();
			repeatFrameCounter = scteFrameRepeatCount;
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

void H264Packetizer::AppendScteData(uint64_t ts, const Buffer& data, uint8_t repeatCount)
{
	scteMessage.AppendData(data.GetData(), data.GetSize());
	scteFrameRepeatCount = repeatFrameCounter = repeatCount;
	scteTimestamp = ts;

	// Clear just the scte payload and retain the 2 byte code, size and 16 bytes UUID
	sei.erase(sei.begin() + 2 + 1 + 16, sei.end());

	// Binary format - SEI NAL (0x06) + Unregistered User data code (0x05) + UUID (16 bytes) + SCTE ID (2 bytes) + TS (8 bytes) + Base64 encoded scte payload (x bytes)
	

	// Modify the payload size at pos 2 (zero indexed) 
	// [0] 0x06 SEI NAL 
	// [1] 0x05 Unregistered user data
	// [2] Payload size
	sei.at(2) = 16 + 2 + 8 + scteMessage.GetSize(); // UUID 16 bytes + SCTE ID (2 bytes) + TS (8 bytes) + Scte payload (x bytes)

	// Insert 2 byte scte message ID
	scteMessageId++;
	sei.insert(sei.end(), (uint8_t*)&scteMessageId, (uint8_t*)&scteMessageId + sizeof(uint16_t));

	// Insert 8 byte timestamp
	sei.insert(sei.end(), (uint8_t*)&scteTimestamp, (uint8_t*)&scteTimestamp + sizeof(uint64_t));

	// scte payload
	sei.insert(sei.end(), scteMessage.GetData(), scteMessage.GetData() + scteMessage.GetSize());
	
	// rbsp_trailing_bits
	sei.push_back(0x80);
}