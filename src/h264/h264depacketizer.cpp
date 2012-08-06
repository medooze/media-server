/* 
 * File:   h264depacketizer.cpp
 * Author: Sergio
 * 
 * Created on 26 de enero de 2012, 9:46
 */

#include "h264depacketizer.h"
#include "media.h"
#include "codecs.h"
#include "rtp.h"
#include "log.h"

/* 3 zero bytes syncword */
//static uint8_t sync_bytes[] = { 0, 0, 0, 1 };


H264Depacketizer::H264Depacketizer() : RTPDepacketizer(MediaFrame::Video,VideoCodec::H264), frame(VideoCodec::H264,0)
{
}

H264Depacketizer::~H264Depacketizer()
{

}
void H264Depacketizer::SetTimestamp(DWORD timestamp)
{
	//Set timestamp
	frame.SetTimestamp(timestamp);
}
void H264Depacketizer::ResetFrame()
{
	//Clear packetization info
	frame.ClearRTPPacketizationInfo();
	//Reset
	memset(frame.GetData(),0,frame.GetMaxMediaLength());
	//Clear length
	frame.SetLength(0);
}

MediaFrame* H264Depacketizer::AddPacket(RTPPacket *packet)
{
	//Set timestamp
	frame.SetTimestamp(packet->GetTimestamp());
	//Add payload
	return AddPayload(packet->GetMediaData(),packet->GetMediaLength());
}

MediaFrame* H264Depacketizer::AddPayload(BYTE* payload, DWORD payload_len)
{
	BYTE nalHeader[4];
	BYTE nal_unit_type;
	BYTE nal_ref_idc;
	BYTE S, E;
	DWORD nalu_size;
	DWORD pos;
	//Check lenght
	if (!payload_len)
		//Exit
		return NULL;

	/* +---------------+
	 * |0|1|2|3|4|5|6|7|
	 * +-+-+-+-+-+-+-+-+
	 * |F|NRI|  Type   |
	 * +---------------+
	 *
	 * F must be 0.
	 */
	nal_ref_idc = (payload[0] & 0x60) >> 5;
	nal_unit_type = payload[0] & 0x1f;

	//printf("[NAL:%x,type:%x]\n", payload[0], nal_unit_type);

	//Check type
	switch (nal_unit_type)
	{
		case 0:
		case 30:
		case 31:
			/* undefined */
			return NULL;
		case 25:
			/* STAP-B		Single-time aggregation packet		 5.7.1 */
			/* 2 byte extra header for DON */
			/** Not supported */
			return NULL;
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
			//Everything goes to the payload
			frame.AddRtpPacket(0,0,payload,payload_len);

			/* Skip STAP-A NAL HDR */
			payload++;
			payload_len--;

			/* STAP-A Single-time aggregation packet 5.7.1 */
			while (payload_len > 2)
			{
				/* Get NALU size */
				nalu_size = (payload[0] << 8) | payload[1];

				/* strip NALU size */
				payload += 2;
				payload_len -= 2;

				//Get nal type
				BYTE nalType = payload[0] & 0x1f;
				//Check it
				if (nalType==0x05)
					//It is intra
					frame.SetIntra(true);

				//Set size
				set4(nalHeader,0,nalu_size);
				//Append data
				//frame.AppendMedia(sync_bytes, sizeof (sync_bytes));
				frame.AppendMedia(nalHeader, sizeof (nalHeader));
				
				//Append NAL
				frame.AppendMedia(payload,nalu_size);
				
				payload += nalu_size;
				payload_len -= nalu_size;
			}
			break;
		case 26:
			/* MTAP16 Multi-time aggregation packet	5.7.2 */
			return NULL;
		case 27:
			/* MTAP24 Multi-time aggregation packet	5.7.2 */
			return NULL;
		case 28:
		case 29:
			/* FU-A	Fragmentation unit	 5.8 */
			/* FU-B	Fragmentation unit	 5.8 */


			/* +---------------+
			 * |0|1|2|3|4|5|6|7|
			 * +-+-+-+-+-+-+-+-+
			 * |S|E|R| Type	   |
			 * +---------------+
			 *
			 * R is reserved and always 0
			 */
			S = (payload[1] & 0x80) == 0x80;
			E = (payload[1] & 0x40) == 0x40;

			/* strip off FU indicator and FU header bytes */
			nalu_size = payload_len-2;

			if (S)
			{
				/* NAL unit starts here */
				BYTE nal_header;

				/* reconstruct NAL header */
				nal_header = (payload[0] & 0xe0) | (payload[1] & 0x1f);

				//Get nal type
				BYTE nalType = nal_header & 0x1f;
				//Check it
				if (nalType==0x05)
					//It is intra
					frame.SetIntra(true);

				//Get init of the nal
				iniFragNALU = frame.GetLength();
				//Set size with start code
				set4(nalHeader,0,1);
				//Append data
				frame.AppendMedia(nalHeader, sizeof (nalHeader));
				//Append NAL header
				frame.AppendMedia(&nal_header,1);
			}

			//Get position
			pos = frame.GetLength();
			//Append data
			frame.AppendMedia(payload+2,nalu_size);
			//Add rtp payload
			frame.AddRtpPacket(pos,nalu_size,payload,2);

			if (E)
			{
				//Get NAL size
				DWORD nalSize = frame.GetLength()-iniFragNALU-4;
				//Set it
				set4(frame.GetData(),iniFragNALU,nalSize);
			}
			//Done
			break;
		default:
			/* 1-23	 NAL unit	Single NAL unit packet per H.264	 5.6 */
			//Check it
			if (nal_unit_type==0x05)
				//It is intra
				frame.SetIntra(true);
			/* the entire payload is the output buffer */
			nalu_size = payload_len;
			//Set size
			set4(nalHeader,0,nalu_size);
			//Append data
			frame.AppendMedia(nalHeader, sizeof (nalHeader));
			//Get current position in frame
			DWORD pos = frame.GetLength();
			//And data
			frame.AppendMedia(payload, nalu_size);
			//Add RTP packet
			frame.AddRtpPacket(pos,nalu_size,NULL,0);
			//Done
			break;
	}

	return &frame;
}

