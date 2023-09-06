#include "H264LayerSelector.h"
#include "h264.h"
		
H264LayerSelector::H264LayerSelector()
{
}

void H264LayerSelector::SelectTemporalLayer(BYTE id)
{
	//Check if its the same
	if (id==nextTemporalLayerId)
		//Do nothing
		return;
	//Log
	UltraDebug("-H264LayerSelector::SelectTemporalLayer() [layerId:%d,prev:%d,current:%d]\n",id,nextTemporalLayerId,temporalLayerId);
	//Set next
	nextTemporalLayerId = id;
}

void H264LayerSelector::SelectSpatialLayer(BYTE id)
{
	//Not supported
}

bool H264LayerSelector::Select(const RTPPacket::shared& packet,bool &mark)
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
		//Check if it is intra
		isIntra = fm.startOfFrame && fm.independent;
		
		Log("-H264LayerSelector::Select() | [ssrc:%u,isIntra:%d,s:%d,independet:%d,baseLayerSync:%d,tid:%d]\n",packet->GetSSRC(),isIntra,fm.startOfFrame,fm.independent,fm.baseLayerSync,fm.temporalLayerId);
		
		//Store current temporal id
		BYTE currentTemporalLayerId = temporalLayerId;

		//Check if we need to upscale temporally
		if (nextTemporalLayerId>temporalLayerId)
		{
			//Check if we can upscale and it is the start of the layer and it is a layer higher than current
			if (fm.baseLayerSync && fm.startOfFrame && fm.temporalLayerId>currentTemporalLayerId && fm.temporalLayerId<=nextTemporalLayerId)
			{
				UltraDebug("-H264LayerSelector::Select() | Upscaling temporalLayerId [id:%d,current:%d,target:%d]\n",fm.temporalLayerId,currentTemporalLayerId,nextTemporalLayerId);
				//Update current layer
				temporalLayerId = fm.temporalLayerId;
				currentTemporalLayerId = temporalLayerId;
			}
		//Check if we need to downscale
		} else if (nextTemporalLayerId<temporalLayerId) {
			//We can only downscale on the end of a frame
			if (packet->GetMark())
			{
				UltraDebug("-H264LayerSelector::Select() | Downscaling temporalLayerId [id:%d,current:%d,target:%d]\n",temporalLayerId,currentTemporalLayerId,nextTemporalLayerId);
				//Update to target layer for next packets
				temporalLayerId = nextTemporalLayerId;
			}
		}

		//If it is not valid for the current layer
		if (currentTemporalLayerId<fm.temporalLayerId)
		{
			UltraDebug("-H264LayerSelector::Select() | dropping packet based on temporalLayerId [current:%d,desc:%d,mark:%d]\n",currentTemporalLayerId,fm.temporalLayerId,packet->GetMark());
			//Drop it
			return false;
		}
	} else {	
		/* +---------------+
		 * |0|1|2|3|4|5|6|7|
		 * +-+-+-+-+-+-+-+-+
		 * |F|NRI|  Type   |
		 * +---------------+
		 *
		 * F must be 0.
		 */
		BYTE nal_unit_type = payload[0] & 0x1f;

		//Debug("-H264 [NAL:%d,type:%d]\n", payload[0], nal_unit_type);

		//Check type
		switch (nal_unit_type)
		{
			case 0:
			case 30:
			case 31:
				/* undefined */
				return false;
			case 25:
				/* STAP-B		Single-time aggregation packet		 5.7.1 */
				/* 2 byte extra header for DON */
				/** Not supported */
				return false;
			case 24:
				/**
				   Figure 7 presents an example of an RTP packet that contains an STAP-
				   A.  The STAP contains two single-time aggregation units, labeled as 1
				   and 2 in the figure.

				       0                   1                   2                   3
				       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				      |                          RTP Header                           |
				      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				      |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
				      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				      |                         NALU 1 Data                           |
				      :                                                               :
				      +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				      |               | NALU 2 Size                   | NALU 2 HDR    |
				      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				      |                         NALU 2 Data                           |
				      :                                                               :
				      |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				      |                               :...OPTIONAL RTP padding        |
				      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

				      Figure 7.  An example of an RTP packet including an STAP-A and two
						 single-time aggregation units
				*/

				/* Skip STAP-A NAL HDR */
				payload++;
				payloadLen--;

				/* STAP-A Single-time aggregation packet 5.7.1 */
				while (payloadLen > 2)
				{
					/* Get NALU size */
					BYTE nalSize = (payload[0] << 8) | payload[1];

					/* strip NALU size */
					payload += 2;
					payloadLen -= 2;

					//Check
					if (!nalSize || nalSize>payloadLen)
						//Error
						break;

					//Get nal type
					BYTE nalType = payload[0] & 0x1f;

					//Get nal data
					[[maybe_unused]] const BYTE *nalData = payload+1;

					//Check if IDR SPS or PPS
					switch (nalType)
					{
						case 0x05:
							//It is intra
							//isIntra = true;
							break;
						case 0x07:
							//Consider it intra also
							isIntra = true;
							//Parse SPS
							//sps.Decode(nalData,nalSize-1);
							break;
						case 0x08:
							//Consider it intra also
							isIntra = true;
							//Parse PPS
							//pps.Decode(nalData,nalSize-1);
							break;
					}

					payload += nalSize;
					payloadLen -= nalSize;
				}
				break;
			case 26:
				/* MTAP16 Multi-time aggregation packet	5.7.2 */
				return false;
			case 27:
				/* MTAP24 Multi-time aggregation packet	5.7.2 */
				return false;
			case 28:
			case 29:
			{
				/* FU-A	Fragmentation unit	 5.8 */
				/* FU-B	Fragmentation unit	 5.8 */

				//Check length
				if (payloadLen < 2)
					return false;

				/* +---------------+
				 * |0|1|2|3|4|5|6|7|
				 * +-+-+-+-+-+-+-+-+
				 * |S|E|R| Type	   |
				 * +---------------+
				 *
				 * R is reserved and always 0
				 */
				bool S = (payload[1] & 0x80) == 0x80;

				/* strip off FU indicator and FU header bytes */
				[[maybe_unused]] BYTE nalSize = payloadLen-2;

				if (S)
				{
					/* NAL unit starts here */
					BYTE nal_header;

					/* reconstruct NAL header */
					nal_header = (payload[0] & 0xe0) | (payload[1] & 0x1f);

					//Get nal type
					BYTE nalType = nal_header & 0x1f;

					//Get nal data
					[[maybe_unused]] const BYTE *nalData = payload+1;

					//Check if IDR SPS or PPS
					switch (nalType)
					{
						case 0x05:
							//It is intra
							isIntra = true;
							break;
						case 0x07:
							//Consider it intra also
							isIntra = true;
							//Parse SPS
							//sps.Decode(nalData,nalSize-1);
							break;
						case 0x08:
							//Consider it intra also
							isIntra = true;
							//Parse PPS
							//pps.Decode(nalData,nalSize-1);
							break;
					}
				}
				//Done
				break;
			}
			default:
			{
				/* 1-23	 NAL unit	Single NAL unit packet per H.264	 5.6 */

				//Get nal data
				[[maybe_unused]] const BYTE *nalData = payload+1;
				//Get nalu size
				[[maybe_unused]] WORD nalSize = payloadLen-1;
				BYTE nalType = nal_unit_type;

				//Check if IDR SPS or PPS
				switch (nalType)
				{
					case 0x05:
						//It is intra
						//isIntra = true;
						break;
					case 0x07:
						//Consider it intra also
						isIntra = true;
						//Parse SPS
						//sps.Decode(nalData,nalSize-1);
						break;
					case 0x08:
						//Consider it intra also
						isIntra = true;
						//Parse PPS
						//pps.Decode(nalData,nalSize-1);
						break;
				}
				//Done
				break;
			}
		}
	} 

	//Debug("-intra:%d\t waitingForIntra:%d\n",isIntra,waitingForIntra);
	
	//If we have to wait for first intra
	if (waitingForIntra)
	{
		//If this is not intra
		if (!isIntra)
			//Discard
			return 0;
		//Stop waiting
		waitingForIntra = 0;
	}
	
	//RTP mark is unchanged
	mark = packet->GetMark();
	
	//Select
	return true;
	
}

 std::vector<LayerInfo> H264LayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	std::vector<LayerInfo> infos;
	
	//If packet has frame markings
	if (packet->HasFrameMarkings())
	{
		//Get it from frame markings
		const auto& fm = packet->GetFrameMarks();
		//Get data from frame marking
		infos.emplace_back(fm.temporalLayerId, fm.layerId);
		//Set key frame flag
		packet->SetKeyFrame(fm.independent);
	} else {
		//Get payload
		const uint8_t* payload = packet->GetMediaData();
		uint32_t payloadLen = packet->GetMediaLength();

		//Check size
		if (payloadLen)
		{
			/* +---------------+
			 * |0|1|2|3|4|5|6|7|
			 * +-+-+-+-+-+-+-+-+
			 * |F|NRI|  Type   |
			 * +---------------+
			 *
			 * F must be 0.
			 */
			 //Check if first nal
			 BYTE nalUnitType = payload[0] & 0x1f;

			//Get nal data
			const BYTE* nalData = payload + 1;

			//Get nalu size
			DWORD nalSize = payloadLen;

			//Debug("-H264 [NAL:%d,type:%d,size:%d]\n", payload[0], nalUnitType, nalSize);

			//Check type
			switch (nalUnitType)
			{
				case 0:
				case 30:
				case 31:
					/* undefined */
					break;
				case 25:
					/* STAP-B		Single-time aggregation packet		 5.7.1 */
					/* 2 byte extra header for DON */
					/** Not supported */
					break;
				case 24:
					/**
					   Figure 7 presents an example of an RTP packet that contains an STAP-
					   A.  The STAP contains two single-time aggregation units, labeled as 1
					   and 2 in the figure.

					       0                   1                   2                   3
					       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
					      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					      |                          RTP Header                           |
					      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					      |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
					      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					      |                         NALU 1 Data                           |
					      :                                                               :
					      +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					      |               | NALU 2 Size                   | NALU 2 HDR    |
					      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					      |                         NALU 2 Data                           |
					      :                                                               :
					      |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					      |                               :...OPTIONAL RTP padding        |
					      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

					      Figure 7.  An example of an RTP packet including an STAP-A and two
							 single-time aggregation units
					*/

					/* Skip STAP-A NAL HDR */
					payload++;
					payloadLen--;

					/* STAP-A Single-time aggregation packet 5.7.1 */
					while (payloadLen > 2)
					{
						/* Get NALU size */
						nalSize = get2(payload, 0);

						/* strip NALU size */
						payload += 2;
						payloadLen -= 2;

						//Check
						if (!nalSize || nalSize > payloadLen)
							//Error
							break;

						//Get nal type
						nalUnitType = payload[0] & 0x1f;
						//Get data
						nalData = payload + 1;

						//Check if IDR SPS or PPS
						switch (nalUnitType)
						{
							case 0x05:
								//It is intra
								packet->SetKeyFrame(true);
								break;
							case 0x07:
								//Consider it intra also
								packet->SetKeyFrame(true);
								//Create sps
								packet->h264SeqParameterSet.emplace();
								//Parse sps
								if (packet->h264SeqParameterSet->Decode(nalData, nalSize - 1))
								{
									//Set dimensions
									packet->SetWidth(packet->h264SeqParameterSet->GetWidth());
									packet->SetHeight(packet->h264SeqParameterSet->GetHeight());
								} else {
									//Remove sps
									packet->h264SeqParameterSet.reset();
								}
								break;
							case 0x08:
								//Consider it intra also
								packet->SetKeyFrame(true);
								//Create pps
								packet->h264PictureParameterSet.emplace();
								//Parse sps
								if (!packet->h264PictureParameterSet->Decode(nalData, nalSize - 1))
								{
									//Remove sps
									packet->h264SeqParameterSet.reset();
								}
								break;
						}

						//Next aggregation nal
						payload += nalSize;
						payloadLen -= nalSize;
					}
					break;
				case 0x05:
					//It is intra
					packet->SetKeyFrame(true);
					break;
				case 0x07:
					//Consider it intra also
					packet->SetKeyFrame(true);
					//Create sps
					packet->h264SeqParameterSet.emplace();
					//Parse sps
					if (packet->h264SeqParameterSet->Decode(nalData, nalSize - 1))
					{
						//Set dimensions
						packet->SetWidth(packet->h264SeqParameterSet->GetWidth());
						packet->SetHeight(packet->h264SeqParameterSet->GetHeight());
					} else {
						//Remove sps
						//Remove sps
						packet->h264SeqParameterSet.reset();
					}
					break;
				case 0x08:
					//Consider it intra also
					packet->SetKeyFrame(true);
					//Create pps
					packet->h264PictureParameterSet.emplace();
					//Parse sps
					if (!packet->h264PictureParameterSet->Decode(nalData, nalSize - 1))
					{
						//Remove sps
						packet->h264SeqParameterSet.reset();
					}
					break;
				default:
					//Do nothing
					break;
			}
		}
	}
	//UltraDebug("-H264LayerSelector::GetLayerIds() | [isKeyFrame:%d,width:%d,height:%d]\n",packet->IsKeyFrame(),packet->GetWidth(),packet->GetHeight());
	
	//Return layer infos
	return infos;
}
	
