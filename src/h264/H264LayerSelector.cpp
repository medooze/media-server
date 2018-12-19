#include "H264LayerSelector.h"
#include "h264.h"
		
H264LayerSelector::H264LayerSelector()
{
	waitingForIntra = true;
}

bool H264LayerSelector::Select(const RTPPacket::shared& packet,bool &mark)
{
	bool isIntra = true;
	//Get payload
	DWORD payloadLen = packet->GetMediaLength();
	BYTE* payload = packet->GetMediaData();
	
	//Check we have data
	if (!payloadLen)
		//Nothing
		return false;
	
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
				BYTE *nalData = payload+1;
				
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
			BYTE nalSize = payloadLen-2;

			if (S)
			{
				/* NAL unit starts here */
				BYTE nal_header;

				/* reconstruct NAL header */
				nal_header = (payload[0] & 0xe0) | (payload[1] & 0x1f);

				//Get nal type
				BYTE nalType = nal_header & 0x1f;
				
				//Get nal data
				BYTE *nalData = payload+1;
				
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
			
			//Get nalu size
			WORD nalSize = payloadLen;
			BYTE nalType = nal_unit_type;
			
			//Get nal data
			BYTE *nalData = payload+1;
			
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
			//Done
			break;
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
