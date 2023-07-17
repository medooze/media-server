#include "H265LayerSelector.h"
		
H265LayerSelector::H265LayerSelector()
{
	waitingForIntra = true;
}

void H265LayerSelector::SelectTemporalLayer(BYTE id)
{
	//Log
	UltraDebug("-H265LayerSelector::SelectTemporalLayer() [layerId:%d,prev:%d,current:%d]\n",id,nextTemporalLayerId,temporalLayerId);
	//Check if its the same
	if (id==nextTemporalLayerId)
		//Do nothing
		return;
	//Set next
	nextTemporalLayerId = id;
}

void H265LayerSelector::SelectSpatialLayer(BYTE id)
{
	//Not supported
}

bool H265LayerSelector::Select(const RTPPacket::shared& packet,bool &mark)
{
	//We only siwtch on SPS/PPS not intra, as we need the SPS/PPS
	bool isIntra = false;
	//Get payload
	DWORD payloadLen = packet->GetMediaLength();
	const BYTE* payload = packet->GetMediaData();
	
	//Check we have data
	if (!payloadLen)
		//Nothing
		return false;
	
	//If packet has frame markings
	if (packet->HasFrameMarkings())
	{
		//Get it from frame markings
		const auto& fm = packet->GetFrameMarks();
		UltraDebug("-H265LayerSelector::GetLayerIds() | Frame Markers: [temporalLayerId: %d, layerId: %d, independent: %s]\n", fm.temporalLayerId, fm.layerId, fm.independent? "true":"false");
		//Check if it is intra
		isIntra = fm.startOfFrame && fm.independent;
		
		Log("-H265LayerSelector::Select() | [ssrc:%u,isIntra:%d,s:%d,independet:%d,baseLayerSync:%d,tid:%d]\n",packet->GetSSRC(),isIntra,fm.startOfFrame,fm.independent,fm.baseLayerSync,fm.temporalLayerId);
		
		//Store current temporal id
		BYTE currentTemporalLayerId = temporalLayerId;

		//Check if we need to upscale temporally
		if (nextTemporalLayerId>temporalLayerId)
		{
			//Check if we can upscale and it is the start of the layer and it is a layer higher than current
			if (fm.baseLayerSync && fm.startOfFrame && fm.temporalLayerId>currentTemporalLayerId && fm.temporalLayerId<=nextTemporalLayerId)
			{
				UltraDebug("-H265LayerSelector::Select() | Upscaling temporalLayerId [id:%d,current:%d,target:%d]\n",fm.temporalLayerId,currentTemporalLayerId,nextTemporalLayerId);
				//Update current layer
				temporalLayerId = fm.temporalLayerId;
				currentTemporalLayerId = temporalLayerId;
			}
		//Check if we need to downscale
		} else if (nextTemporalLayerId<temporalLayerId) {
			//We can only downscale on the end of a frame
			if (packet->GetMark())
			{
				UltraDebug("-H265LayerSelector::Select() | Downscaling temporalLayerId [id:%d,current:%d,target:%d]\n",temporalLayerId,currentTemporalLayerId,nextTemporalLayerId);
				//Update to target layer for next packets
				temporalLayerId = nextTemporalLayerId;
			}
		}

		//If it is not valid for the current layer
		if (currentTemporalLayerId<fm.temporalLayerId)
		{
			UltraDebug("-H265LayerSelector::Select() | dropping packet based on temporalLayerId [current:%d,desc:%d,mark:%d]\n",currentTemporalLayerId,fm.temporalLayerId,packet->GetMark());
			//Drop it
			return false;
		}
	}
	else
	{	
		BYTE nalUnitType;
		BYTE nuh_layer_id;
		BYTE nuh_temporal_id_plus1;
 		if(!H265DecodeNalHeader(payload, payloadLen, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1))
		{
			Error("-H265LayerSelector::Select() | Failed to decode H265 Nal Header!\n");
			return false;
		}

		UltraDebug("-H265LayerSelector::Select() | [NAL:0x%02x%02x,type:%d]\n", payload[0], payload[1], nalUnitType);

		//Check if IDR/VPS/SPS/PPS, set Intra
		if (H265IsIntra(nalUnitType))
		{
			isIntra = true;
		}

		//Check type
		switch (nalUnitType)
		{
			case HEVC_RTP_NALU_Type::AUD:		//35
			case HEVC_RTP_NALU_Type::EOS:		//36
			case HEVC_RTP_NALU_Type::EOB:		//37
			case HEVC_RTP_NALU_Type::FD:		//38
				//UltraDebug("-H265 Un-defined/implemented NALU, skipping");
				/* undefined */
				return false;
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
					DWORD nalSize = get2(payload, 0);

					/* strip NALU size */
					payload += nalSizeLengthInAP;
					payloadLen -= nalSizeLengthInAP;

					//Check
					if (!nalSize || nalSize > payloadLen)
					{
						Error("-H265: RTP AP depacketizer error! nalSize: %d, payloadLen: %d\n", nalSize, payloadLen);
						return false;
					}

 					if(!H265DecodeNalHeader(payload, nalSize, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1))
					{
						Error("-H265LayerSelector::Select() | Failed to decode H265 Nal Header in AP!\n");
						return false;
					}

					//Check if IDR/VPS/SPS/PPS, set Intra
					if (H265IsIntra(nalUnitType))
					{
						isIntra = true;
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
				const size_t nalAndFuHeadersLength = HEVCParams::RTP_NAL_HEADER_SIZE + HEVCParams::RTP_NAL_FU_HEADER_SIZE;
				if (payloadLen < nalAndFuHeadersLength)
				{
					Error("- H265: payloadLen (%d) is smaller than normal nal and FU header len (%d), skipping this packet\n", payloadLen, nalAndFuHeadersLength);
					return false;
				}

				bool S = (payload[2] & 0x80) == 0x80;
				//Get real nal type
				nalUnitType = payload[2] & 0b0011'1111;

				//if it is the start fragment of the nal unit
				if (S)
				{
					//Check if IDR/VPS/SPS/PPS, set Intra
					if (H265IsIntra(nalUnitType))
					{
						isIntra = true;
					}
				}
				break;
			}
			default:
				//Check if IDR/VPS/SPS/PPS, set Intra
				if (H265IsIntra(nalUnitType))
				{
					isIntra = true;
				}
				break;
		}
	}

	Debug("-intra:%d\t waitingForIntra:%d\n",isIntra,waitingForIntra);
	
	//If we have to wait for first intra
	if (waitingForIntra)
	{
		//If this is not intra
		if (!isIntra)
			//Discard
			return false;
		//Stop waiting
		waitingForIntra = 0;
	}
	
	//RTP mark is unchanged
	mark = packet->GetMark();
	
	//Select
	return true;
}

void H265LayerSelector::SetPacketParameters(const RTPPacket::shared& packet, BYTE nalUnitType, const BYTE* nalData, DWORD nalSize /*includ Nal Header*/)
{
	switch (nalUnitType)
	{
		case HEVC_RTP_NALU_Type::VPS:
			packet->h265VideoParameterSet.emplace();
			//Parse vps
			if (!packet->h265VideoParameterSet->Decode(nalData, nalSize - HEVCParams::RTP_NAL_HEADER_SIZE))
			{
				//Remove vps
				packet->h265VideoParameterSet.reset();
			}
			break;
		case HEVC_RTP_NALU_Type::SPS:
			packet->h265SeqParameterSet.emplace();
			//Parse sps
			if (packet->h265SeqParameterSet->Decode(nalData, nalSize - HEVCParams::RTP_NAL_HEADER_SIZE))
			{
				//Set dimensions
				packet->SetWidth(packet->h265SeqParameterSet->GetWidth());
				packet->SetHeight(packet->h265SeqParameterSet->GetHeight());
			}
			else
			{
				//Remove sps
				packet->h265SeqParameterSet.reset();
			}
			break;
		case HEVC_RTP_NALU_Type::PPS:
			packet->h265PictureParameterSet.emplace();
			//Parse pps
			if (!packet->h265PictureParameterSet->Decode(nalData, nalSize - HEVCParams::RTP_NAL_HEADER_SIZE))
			{
				//Remove sps
				packet->h265PictureParameterSet.reset();
			}
			break;
	}
}

std::vector<LayerInfo> H265LayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	UltraDebug("-H265LayerSelector::GetLayerIds() | start to get Layer ids info\n");
	std::vector<LayerInfo> infos;
	
	 //If packet has frame markings
	if (packet->HasFrameMarkings())
	{
		//Get it from frame markings
		const auto& fm = packet->GetFrameMarks();
		UltraDebug("-H265LayerSelector::GetLayerIds() | Frame Markers: [temporalLayerId: %d, layerId: %d, independent: %s]\n", fm.temporalLayerId, fm.layerId, fm.independent? "true":"false");
		//Get data from frame marking
		infos.emplace_back(fm.temporalLayerId, fm.layerId);
		//Set key frame flag
		packet->SetKeyFrame(fm.independent);
	}
	else 
	{
		//Get payload
		const uint8_t* payload = packet->GetMediaData();
		uint32_t payloadLen = packet->GetMediaLength();

		//Check size
		if (payloadLen)
		{
			BYTE nalUnitType;
			BYTE nuh_layer_id;
			BYTE nuh_temporal_id_plus1;
 			if(!H265DecodeNalHeader(payload, payloadLen, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1))
			{
				Error("-H265LayerSelector::GetLayerIds() | Failed to decode H265 Nal Header!\n");
				return infos;
			}

			//Get nalu size
			DWORD nalSize = payloadLen;

			Debug("-H265 [NAL:0x%02x%02x,type:%d,size:%d]\n", payload[0], payload[1], nalUnitType, nalSize);

			//Check type
			switch (nalUnitType)
			{
				case HEVC_RTP_NALU_Type::AUD:		//35
				case HEVC_RTP_NALU_Type::EOS:		//36
				case HEVC_RTP_NALU_Type::EOB:		//37
				case HEVC_RTP_NALU_Type::FD:		//38
					/* undefined */
					break;
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
							break;
						}

 						if(!H265DecodeNalHeader(payload, payloadLen, nalUnitType, nuh_layer_id, nuh_temporal_id_plus1))
						{
							Error("-H265LayerSelector::Select() | Failed to decode H265 Nal Header in AP!\n");
							break;
						}
						//Check if IDR/VPS/SPS/PPS, set Intra
						if (H265IsIntra(nalUnitType))
						{
							packet->SetKeyFrame(true);
						}
						SetPacketParameters(packet, nalUnitType, payload + HEVCParams::RTP_NAL_HEADER_SIZE, nalSize);

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
					const size_t nalAndFuHeadersLength = HEVCParams::RTP_NAL_HEADER_SIZE + HEVCParams::RTP_NAL_FU_HEADER_SIZE;
					if (payloadLen < nalAndFuHeadersLength)
					{
						Error("- H265: payloadLen (%d) is smaller than normal nal and FU header len (%d), skipping this packet\n", payloadLen, nalAndFuHeadersLength);
						break;
					}

					bool S = (payload[2] & 0x80) == 0x80;
					//Get real nal type
					nalUnitType = payload[2] & 0b0011'1111;

					//if it is the start fragment of the nal unit
					if (S)
					{
						//Check if IDR/VPS/SPS/PPS, set Intra
						if (H265IsIntra(nalUnitType))
						{
							packet->SetKeyFrame(true);
						}
					}
					break;
				}
				default:
					//Check if IDR/VPS/SPS/PPS, set Intra
					if (H265IsIntra(nalUnitType))
					{
						packet->SetKeyFrame(true);
					}
					SetPacketParameters(packet, nalUnitType, payload + HEVCParams::RTP_NAL_HEADER_SIZE, nalSize);
					break;
			}
			//set tld and lid
			//Get data from frame marking
			//infos.emplace_back(nuh_temporal_id_plus1 - 1, nuh_layer_id);
			//UltraDebug("-H265LayerSelector::set info {%d, %d}, size: %d}\n", nuh_temporal_id_plus1-1, nuh_layer_id, infos.size());
		}
	}

	////debug:
	//packet->SetWidth(1280);
	//packet->SetHeight(720);

	UltraDebug("-H265LayerSelector::GetLayerIds() | [isKeyFrame:%d,width:%d,height:%d]\n",packet->IsKeyFrame(),packet->GetWidth(),packet->GetHeight());
	//UltraDebug("-H265LayerSelector::GetLayerIds() | infos[0] = {%d, %d}\n", infos[0].);
	
	//Return layer infos
	return infos;
}
	
