/*
 * File:   h265depacketizer.cpp
 * Author: Sergio
 *
 * Created on 26 de enero de 2012, 9:46
 */

#include "H265Depacketizer.h"
#include "media.h"
#include "codecs.h"
#include "rtp.h"
#include "log.h"

H265Depacketizer::H265Depacketizer(bool annexB_in) :
	RTPDepacketizer(MediaFrame::Video, VideoCodec::H265),
	frame(VideoCodec::H265, 0),
	annexB(annexB_in)
{
	//Set clock rate
	frame.SetClockRate(90000);
}

H265Depacketizer::~H265Depacketizer()
{
}

void H265Depacketizer::AddCodecConfig()
{
	//Set config size
	frame.AllocateCodecConfig(config.GetSize());
	//Serialize
	config.Serialize(frame.GetCodecConfigData(), frame.GetCodecConfigSize());
}

void H265Depacketizer::ResetFrame()
{
	//Clear packetization info
	frame.Reset();
	//Clear config
	config.ClearVideoParameterSets();
	config.ClearSequenceParameterSets();
	config.ClearPictureParameterSets();
	//No fragments
	iniFragNALU = 0;
	startedFrag = false;
}

MediaFrame* H265Depacketizer::AddPacket(const RTPPacket::shared& packet)
{
	UltraDebug("-H265Depacketizer::AddPacket()\n");

	//Get timestamp in ms
	auto ts = packet->GetExtTimestamp();
	//Check it is from same packet
	if (frame.GetTimeStamp() != ts)
		//Reset frame
		ResetFrame();
	//If not timestamp
	if (frame.GetTimeStamp() == (DWORD)-1)
	{
		//Set timestamp
		frame.SetTimestamp(ts);
		//Set clock rate
		frame.SetClockRate(packet->GetClockRate());
		//Set time
		frame.SetTime(packet->GetTime());
		//Set sender time
		frame.SetSenderTime(packet->GetSenderTime());
	}
	//Set SSRC
	frame.SetSSRC(packet->GetSSRC());
	//Add payload
	AddPayload(packet->GetMediaData(), packet->GetMediaLength());
	//If it is last return frame
	if (!packet->GetMark())
		return nullptr;
	//Add codec config
	AddCodecConfig();

	//Return frame
	return &frame;
}

bool DecodeNalHeader(const BYTE* payload, DWORD payloadLen, BYTE& nalUnitType, BYTE& nuh_layer_id, BYTE& nuh_temporal_id_plus1)
{
	//Check length
	if (payloadLen<HEVCParams::RTP_NAL_HEADER_SIZE)
		//Exit
		return false;

	/* 
	*   +-------------+-----------------+
	*   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
	*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*   |F|   Type    |  LayerId  | TID |
	*   +-------------+-----------------+
	*
	* F must be 0.
	*/

	nalUnitType = (payload[0] & 0x7e) >> 1;
	nuh_layer_id = ((payload[0] & 0x1) << 5) + ((payload[1] & 0xf8) >> 3);
	nuh_temporal_id_plus1 = payload[1] & 0x7;
	//UltraDebug("-H265 : [NAL header:0x%02x%02x,type:%d,layer_id:%d, temporal_id:%d]\n", payload[0], payload[1], nalUnitType, nuh_layer_id, nuh_temporal_id_plus1);
	return true;
}

bool H265Depacketizer::AddSingleNalUnitPayload(const BYTE* nalUnit, DWORD nalSize /*include nalHeader*/)
{
	BYTE nalUnitType{0}, nuh_layer_id{0}, nuh_temporal_id_plus1{0};
	if (!DecodeNalHeader(nalUnit, nalSize, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1))
	{
		Error("-H265: Failed to decode NAL Header!\n");
		return false;
	}

	BYTE nalHeaderPreffix[4]; // set as AnenexB or not
	/* 4.4.1.  Single NAL Unit Packets */
	/* 
		0                   1                   2                   3
		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|           PayloadHdr          |      DONL (conditional)       |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                                                               |
		|                  NAL unit payload data                        |
		|                                                               |
		|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                               :...OPTIONAL RTP padding        |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	/* @Zita Liao: sprop-max-don-diff is considered to be absent right now (so no DONL). Need to extract from SDP */
	/* the entire payload is the output buffer */

	const BYTE* nalData = nalUnit + HEVCParams::RTP_NAL_HEADER_SIZE;
	const size_t nalDataSize = nalSize - HEVCParams::RTP_NAL_HEADER_SIZE;
	//Check if IDR/VPS/SPS/PPS, set Intra
	if ((nalUnitType >= HEVC_RTP_NALU_Type::BLA_W_LP && nalUnitType <= HEVC_RTP_NALU_Type::CRA_NUT)
		|| (nalUnitType == HEVC_RTP_NALU_Type::VPS)
		|| (nalUnitType == HEVC_RTP_NALU_Type::SPS)
		|| (nalUnitType == HEVC_RTP_NALU_Type::PPS))
	{
		frame.SetIntra(true);
	}

	switch (nalUnitType)
	{
		case HEVC_RTP_NALU_Type::VPS:			// 32
		{
			H265VideoParameterSet vps;
			if (vps.Decode(nalData, nalDataSize))
			{
				auto& profileTierLevel = vps.GetProfileTierLevel();
				config.SetConfigurationVersion(1);
				config.SetGeneralProfileSpace(profileTierLevel.GetGeneralProfileSpace());
				config.SetGeneralTierFlag(profileTierLevel.GetGeneralTierFlag());
				config.SetGeneralProfileIdc(profileTierLevel.GetGeneralProfileIdc());
				config.SetGeneralProfileCompatibilityFlags(profileTierLevel.GetGeneralProfileCompatibilityFlags());
				config.SetGeneralConstraintIndicatorFlags(profileTierLevel.GetGeneralConstraintIndicatorFlags());
				config.SetGeneralLevelIdc(profileTierLevel.GetGeneralLevelIdc());
				config.SetNALUnitLengthSizeMinus1(sizeof(nalHeaderPreffix) - 1);

				//Add full nal to config
				config.AddVideoParameterSet(nalUnit,nalSize);
			}
			else
			{
				Error("-H265: Decode of SPS failed!\n");
			}
			break;
		}
		case HEVC_RTP_NALU_Type::SPS:			// 33
		{
			//Parse sps
			H265SeqParameterSet sps;
			if (sps.Decode(nalData,nalDataSize))
			{
				//Set dimensions
				frame.SetWidth(sps.GetWidth());
				frame.SetHeight(sps.GetHeight());
	
				UltraDebug("-H265 frame (with cropping) size [width: %d, frame height: %d]\n", sps.GetWidth(), sps.GetHeight());

				//Add full nal to config
				config.AddSequenceParameterSet(nalUnit,nalSize);
			}
			else
			{
				Error("-H265: Decode of SPS failed!\n");
			}
			break;
		}
		case HEVC_RTP_NALU_Type::PPS:			// 34
			//Add full nal to config
			config.AddPictureParameterSet(nalUnit,nalSize);
			break;
		default:
			//Debug("-H265 : Nothing to do for this NaluType nalu. Just forwarding it.(nalUnitType: %d, nalSize: %d)\n", nalUnitType, nalSize);
			break;
	}

	//Check if doing annex b
	if (annexB)
		//Set annex b start code
		set4(nalHeaderPreffix, 0, AnnexBStartCode);
	else
		//Set size
		set4(nalHeaderPreffix, 0, nalSize);
	//Append data
	frame.AppendMedia(nalHeaderPreffix, sizeof(nalHeaderPreffix));
	//Append data and get current post
	auto pos = frame.AppendMedia(nalUnit, nalSize);
	//Add RTP packet
	if (nalSize >= RTPPAYLOADSIZE)
	{
		Error("-H265: nalSize(%d) is larger than RTPPAYLOADSIZE (%d)!\n", nalSize, RTPPAYLOADSIZE);
		return false;
	}
	else
	{
		frame.AddRtpPacket(pos, nalSize, nullptr, 0);
	}
	return true;
}

MediaFrame* H265Depacketizer::AddPayload(const BYTE* payload, DWORD payloadLen)
{
	BYTE nalHeaderPreffix[4]; // set as AnenexB or not
	DWORD pos;

	BYTE nalUnitType{0}, nuh_layer_id{0}, nuh_temporal_id_plus1{0};
	if (!DecodeNalHeader(payload, payloadLen, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1))
	{
		Error("-H265: Failed to decode NAL Header!\n");
		return nullptr;
	}

	//Get nalu size
	DWORD nalSize = payloadLen;

	UltraDebug("-H265Depacketizer::AddPayload() | [NAL header:0x%02x%02x,type:%d,layer_id:%d, temporal_id:%d, size:%d]\n", payload[0], payload[1], nalUnitType, nuh_layer_id, nuh_temporal_id_plus1, nalSize);

	//Check type
	switch (nalUnitType)
	{
		case HEVC_RTP_NALU_Type::AUD:		//35
		case HEVC_RTP_NALU_Type::EOS:		//36
		case HEVC_RTP_NALU_Type::EOB:		//37
		case HEVC_RTP_NALU_Type::FD:		//38
			Warning("-H265 Un-defined/implemented NALU, skipping");
			/* undefined */
			return nullptr;
		case HEVC_RTP_NALU_Type::UNSPEC48_AP:	//48 
		{
			/**
			Figure 7 presents an example of an AP that contains two aggregation
			units, labeled as 1 and 2 in the figure, without the DONL and DOND
			fields being present.

			0                   1                   2                   3
			0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|                          RTP Header                           |
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|   PayloadHdr (Type=48)        |         NALU 1 Size           |
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|          NALU 1 HDR           |                               |
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+         NALU 1 Data           |
			|                   . . .                                       |
			|                                                               |
			+               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|  . . .        | NALU 2 Size                   | NALU 2 HDR    |
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			| NALU 2 HDR    |                                               |
			+-+-+-+-+-+-+-+-+              NALU 2 Data                      |
			|                   . . .                                       |
			|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|                               :...OPTIONAL RTP padding        |
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

			Figure 7: An Example of an AP Packet Containing Two Aggregation
			Units without the DONL and DOND Fields

			*/

			// DONL/DOND is not supported at the moment

			/* Skip PayloadHdr */
			payload += HEVCParams::RTP_NAL_HEADER_SIZE;
			payloadLen -= HEVCParams::RTP_NAL_HEADER_SIZE;

			/* 4.4.2.  Aggregation Packets (APs) */
			const size_t nalSizeLengthInAP = 2; // 2B to indicate NULU sise
			while (payloadLen > nalSizeLengthInAP)
			{
				/* Get NALU size */
				nalSize = get2(payload, 0);

				/* strip NALU size */
				payload += nalSizeLengthInAP;
				payloadLen -= nalSizeLengthInAP;

				//Check
				if (!nalSize || nalSize > payloadLen)
				{
					Error("-H265: RTP AP depacketizer error! nalSize: %d, payloadLen: %d\n", nalSize, payloadLen);
					return nullptr;
				}

				if (!AddSingleNalUnitPayload(payload, nalSize))
				{
					Error("-H265: Failed to add Nal Unit payload in AP RTP packet!\n");
					return nullptr;
				}

				payload += nalSize;
				payloadLen -= nalSize;
			}
			break;
		}
		case HEVC_RTP_NALU_Type::UNSPEC49_FU: 
		{
			/*
			  0                   1                   2                   3
			  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 |    PayloadHdr (Type=49)       |   FU header   | DONL (cond)   |
			 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
			 | DONL (cond)   |                                               |
			 |-+-+-+-+-+-+-+-+                                               |
			 |                         FU payload                            |
			 |                                                               |
			 |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 |                               :...OPTIONAL RTP padding        |
			 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 FU header:
			 +---------------+
			 |0|1|2|3|4|5|6|7|
			 +-+-+-+-+-+-+-+-+
			 |S|E|  FuType   |
			 +---------------+
			*/

			// DONL is not supported at the moment

			//Check length is larger then RTP header + FU header
			const size_t nalAndFuHeadersLength = HEVCParams::RTP_NAL_HEADER_SIZE + 1;
			if (payloadLen < nalAndFuHeadersLength)
			{
				Error("- H265: payloadLen (%d) is smaller than normal nal and FU header len (%zu), skipping this packet\n", payloadLen, nalAndFuHeadersLength);
				return nullptr;
			}

			bool S = (payload[2] & 0x80) == 0x80;
			bool E = (payload[2] & 0x40) == 0x40;
			//Get real nal type
			nalUnitType = payload[2] & 0b0011'1111;

			/* strip off FU indicator and FU header bytes */
			nalSize = payloadLen - nalAndFuHeadersLength;

			//if it is the start fragment of the nal unit
			if (S)
			{
				/* NAL unit starts here */
				/* Construct Nal Header, by replacing with the real nal type */
				/* 
				*   +-------------+-----------------+
				*   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
				*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				*   |F|   Type    |  LayerId  | TID |
				*   +-------------+-----------------+
				*
				* F must be 0.
				*/
				std::array<BYTE, 2> fragNalHeader;
				fragNalHeader[0] = (payload[0] & 0b0000'0001) | (nalUnitType << 1); 
				fragNalHeader[1] = payload[1];

				//Check if IDR/VPS/SPS/PPS, set Intra
				if((nalUnitType >= HEVC_RTP_NALU_Type::BLA_W_LP && nalUnitType <= HEVC_RTP_NALU_Type::CRA_NUT)
					|| (nalUnitType == HEVC_RTP_NALU_Type::VPS)
					|| (nalUnitType == HEVC_RTP_NALU_Type::SPS)
					|| (nalUnitType == HEVC_RTP_NALU_Type::PPS))
				{
					frame.SetIntra(true);
				}

				//Get init of the nal
				iniFragNALU = frame.GetLength();
				//Set empty header (non-AnnexB), will be set later
				set4(nalHeaderPreffix, 0, 0);
				//Append data
				frame.AppendMedia(nalHeaderPreffix, sizeof(nalHeaderPreffix));
				//Append NAL header
				frame.AppendMedia(fragNalHeader.data(), fragNalHeader.size());
				//We have a start frag
				startedFrag = true;
			}

			//If we didn't receive a start frag
			if (!startedFrag)
			{
				Error("-H265: received FU RTP packets without Start FU packet!\n");
				return nullptr;
			}

			//Append data and get current post
			pos = frame.AppendMedia(payload + nalAndFuHeadersLength, nalSize);
			//Add rtp payload
			frame.AddRtpPacket(pos, nalSize, payload, nalAndFuHeadersLength);

			//If it is the end fragment of the nal unit
			if (E)
			{
				// overwrite nalHeaderPreffix with nal size
				// Ensure it is valid
				if (iniFragNALU + 4 > frame.GetLength())
				{
					Error("-H265: RTP FU RTP packetizer error!\n");
					return nullptr;
				}
				//Get NAL size
				DWORD nalSize = frame.GetLength() - iniFragNALU - 4;
				//Check if doing annex b
				if (annexB)
					//Set annex b start code
					set4(frame.GetData(), iniFragNALU, AnnexBStartCode);
				else
					set4(frame.GetData(), iniFragNALU, nalSize);
				//Done with fragment
				iniFragNALU = 0;
				startedFrag = false;
			}
			//Done
			break;
		}
		default:
			if (!AddSingleNalUnitPayload(payload, payloadLen))
			{
				Error("-H265: Failed to add Nal Unit payload\n");
				return nullptr;
			}
			break;
	}

	return &frame;
}

