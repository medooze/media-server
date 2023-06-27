#include "H264Packetizer.h"

#include "video.h"
#include "h264/h264.h"

void OnH264Nal(VideoFrame& frame, AVCDescriptor &config, bool& noPPSInFrame, bool& noSPSInFrame, BufferReader& reader)
{
	//Return if current NAL is empty
	if (!reader.GetLeft())
		return;

	//Get nal size
	uint32_t nalSize = reader.GetLeft();
	//Get nal unit
	const uint8_t* nalUnit = reader.PeekData();
	//Nal data
	const uint8_t* nalData = nalUnit + 1;

	//Empty prefix
	uint8_t preffix[4] = {};


	/* +---------------+
	 * |0|1|2|3|4|5|6|7|
	 * +-+-+-+-+-+-+-+-+
	 * |F|NRI|  Type   |
	 * +---------------+
	 */
	uint8_t nri		= nalUnit[0] & 0b0'11'00000;
	uint8_t nalUnitType	= nalUnit[0] & 0b0'00'11111;

	//UltraDebug("-SRTConnection::OnVideoData() | Got nal [type:%d,size:%d]\n", nalUnitType, nalSize);

	//Check if IDR SPS or PPS
	switch (nalUnitType)
	{
		case 0x05:
			//It is intra
			frame.SetIntra(true);
			[[fallthrough]];
		case 0x01:
			//Either it is an iframe and we have valid SPS/PPS from previous frames, or it is a non-iframe with SPS/PPS
			//Ensure we do this only once
			if (frame.IsIntra() && !frame.HasCodecConfig() && config.GetNumOfPictureParameterSets() && config.GetNumOfSequenceParameterSets())
			{
				const uint8_t spsNum = config.GetNumOfSequenceParameterSets();
				// Get total sps size
				// Can improve by adding AVCDescriptorconfig::GetSPSTotolSize() in media-server
				uint64_t spsTotalSize = 0;
				for (uint8_t i = 0; i < spsNum; i++)
				{
					spsTotalSize += config.GetSequenceParameterSetSize(i);
				}

				const uint8_t ppsNum = config.GetNumOfPictureParameterSets();
				// Get total pps size
				// Can improve by adding AVCDescriptorconfig::GetPPSTotolSize() in media-server
				uint64_t ppsTotalSize = 0;
				for (uint8_t i = 0; i < ppsNum; i++)
				{
					ppsTotalSize += config.GetPictureParameterSetSize(i);
				}

				//UltraDebug("-SRTConnection::OnVideoData() | SPS [num: %d, size: %d], PPS [num: %d, size: %d]\n", spsNum, spsTotalSize, ppsNum, ppsTotalSize);
				//Allocate enought space to prevent further reallocations
				// preffix num: spsNum + ps_num + 1
				frame.Alloc(frame.GetLength() + nalSize + spsTotalSize + ppsTotalSize + sizeof(preffix) * (spsNum + ppsNum + 1));

				// Add SPS
				for (uint8_t i = 0; i < spsNum; i++)
				{
					const uint8_t* spsData = config.GetSequenceParameterSet(i);
					uint32_t spsSize = config.GetSequenceParameterSetSize(i);
					//Set size in preffix
					set4(preffix, 0, spsSize);
					//Add nal preffix
					frame.AppendMedia(preffix, sizeof(preffix));
					//Add SPS
					auto pos = frame.AppendMedia(spsData, spsSize);
					//Single packetization
					frame.AddRtpPacket(pos, spsSize);
				}

				// Add PPS
				for (uint8_t i = 0; i < ppsNum; i++)
				{
					const uint8_t* ppsData = config.GetPictureParameterSet(i);
					uint32_t ppsSize = config.GetPictureParameterSetSize(i);
					//Set size in preffix
					set4(preffix, 0, ppsSize);
					//Add nal preffix
					frame.AppendMedia(preffix, sizeof(preffix));
					//Add PPS
					auto pos = frame.AppendMedia(ppsData, ppsSize);
					//Single packetization
					frame.AddRtpPacket(pos, ppsSize);
				}

				//Serialize config in to frame
				frame.AllocateCodecConfig(config.GetSize());
				config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
			}
			
			break;
		case 0x07: // SPS
			//Check nal data size
			if (nalSize < 4)
				break;

			//Set config
			config.SetConfigurationVersion(1);
			config.SetAVCProfileIndication(nalData[0]);
			config.SetProfileCompatibility(nalData[1]);
			config.SetAVCLevelIndication(nalData[2]);
			config.SetNALUnitLength(sizeof(preffix) - 1);

			//Reset previous PPS only on the 1st PPS in current frame
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
				H264SeqParameterSet sps;
				if (sps.Decode(nalData, nalSize - 1))
				{
					//Set dimensions
					frame.SetWidth(sps.GetWidth());
					frame.SetHeight(sps.GetHeight());
				}
			}
			//We consider frames having an SPS/PPS as intra due to intra frame refresh
			frame.SetIntra(true);
			//Don't do anything else yet
			return;
		case 0x08: // PPS
			//Reset previous SPS only on the 1st SPS in current frame
			if (noPPSInFrame)
			{
				//UltraDebug("-SRTConnection::OnVideoData() | Clear cached PPS\n");
				config.ClearPictureParameterSets();
				noPPSInFrame = false;
			}

			//Add full nal to config
			config.AddPictureParameterSet(nalUnit, nalSize);
			//We consider frames having an SPS/PPS as intra due to intra frame refresh
			frame.SetIntra(true);
			//Don't do anything else yet
			return;
		case 0x09:
			//Ignore fame acces unit delimiters
			return;
	}

	
	//Set NAL size on preffix
	set4(preffix, 0, nalSize);

	//Allocate enought space to prevent further reallocations
	frame.Alloc(frame.GetLength() + nalSize + sizeof(preffix));

	//Add nal start
	frame.AppendMedia(preffix, sizeof(preffix));

	//Check nal start in frame
	auto pos = frame.AppendMedia(reader);

	//If it is small
	if (nalSize < RTPPAYLOADSIZE)
	{
		//Single packetization
		frame.AddRtpPacket(pos, nalSize);
		return;
	}

	//Otherwise use FU-A
	/*
	* The FU indicator octet has the following format:
	*
	*       +---------------+
	*       |0|1|2|3|4|5|6|7|
	*       +-+-+-+-+-+-+-+-+
	*       |F|NRI|  Type   |
	*       +---------------+
	*
	* The FU header has the following format:
	*
	*      +---------------+
	*      |0|1|2|3|4|5|6|7|
	*      +-+-+-+-+-+-+-+-+
	*      |S|E|R|  Type   |
	*      +---------------+
	*/
	uint8_t nalHeader[2] = { nri | 28u, 0b100'00000 | nalUnitType };
	//Pending uint8_ts
	uint32_t pending = nalSize;
	//Skip payload nal header
	pending--;
	pos++;
	//Split it
	while (pending)
	{
		int len = std::min<uint32_t>(RTPPAYLOADSIZE - 2, pending);
		//Read it
		pending -= len;
		//If all added
		if (!pending)
			//Set end mark
			nalHeader[1] |= 0b010'00000;
		//Add packetization
		frame.AddRtpPacket(pos, len, nalHeader, sizeof(nalHeader));
		//Not first
		nalHeader[1] &= 0b011'11111;
		//Move start
		pos += len;
	}
}

std::unique_ptr<VideoFrame> H264Packetizer::ProcessAU(BufferReader reader)
{
	//UltraDebug("-H264Packetizer::ProcessAU() | H264 AU [len:%d]\n", reader.GetLeft());
	
	//Alocate enought data
	auto frame = std::make_unique<VideoFrame>(VideoCodec::H264, reader.GetSize());
	bool noPPSInFrame = true;
	bool noSPSInFrame = true;

	NalSliceAnnexB(std::move(reader), [&](auto reader) {
		OnH264Nal(*frame, config, noPPSInFrame, noSPSInFrame, reader);
	});

	//Check if we have new width and heigth
	if (frame->GetWidth() && frame->GetHeight())
	{
		//Update cache
		width = frame->GetWidth();
		height = frame->GetHeight();
	} else {
		//Update from cache
		frame->SetWidth(width);
		frame->SetHeight(height);
	}

	return frame;
}
