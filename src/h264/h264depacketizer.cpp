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
#include "h264.h"


H264Depacketizer::H264Depacketizer(bool annexB) :
	RTPDepacketizer(MediaFrame::Video,VideoCodec::H264),
	frame(VideoCodec::H264,0),
	annexB(annexB)
{
	//Set clock rate
	frame.SetClockRate(90000);
}

H264Depacketizer::~H264Depacketizer()
{
}

void H264Depacketizer::ResetFrame()
{
	//Clear packetization info
	frame.Reset();
	//Clear config
	config.ClearSequenceParameterSets();
	config.ClearPictureParameterSets();
	//No fragments
	iniFragNALU = 0;
	startedFrag = false;
}

MediaFrame* H264Depacketizer::AddPacket(const RTPPacket::shared& packet)
{
	//Get timestamp in ms
	auto ts = packet->GetExtTimestamp();
	//Check it is from same packet
	if (frame.GetTimeStamp()!=ts)
		//Reset frame
		ResetFrame();
	//If not timestamp
	if (frame.GetTimeStamp()==(DWORD)-1)
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
	AddPayload(packet->GetMediaData(),packet->GetMediaLength());
	//If it is last return frame
	if (!packet->GetMark())
		return NULL;
	//Set config size
	frame.AllocateCodecConfig(config.GetSize());
	//Serialize
	config.Serialize(frame.GetCodecConfigData(),frame.GetCodecConfigSize());
	//Return frame
	return &frame;
}

MediaFrame* H264Depacketizer::AddPayload(const BYTE* payload, DWORD payloadLen)
{
	H264SeqParameterSet sps;
	BYTE nalHeader[4];
	BYTE S, E;
	DWORD pos;
	//Check lenght
	if (!payloadLen)
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
	// BYTE nal_ref_idc = (payload[0] & 0x60) >> 5;
	BYTE nalUnitType = payload[0] & 0x1f;
	
	//Get nal data
	const BYTE *nalData = payload+1;
	
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

			/* Skip STAP-A NAL HDR */
			payload++;
			payloadLen--;

			/* STAP-A Single-time aggregation packet 5.7.1 */
			while (payloadLen > 2)
			{
				/* Get NALU size */
				nalSize = get2(payload,0);

				/* strip NALU size */
				payload += 2;
				payloadLen -= 2;
				
				//Check
				if (!nalSize || nalSize>payloadLen)
					//Error
					break;

				//Get nal type
				nalUnitType = payload[0] & 0x1f;
				//Get data
				nalData = payload+1;
				
				//Check if IDR SPS or PPS
				switch (nalUnitType)
				{
					case 0x05:
						//It is intra
						frame.SetIntra(true);
						break;
					case 0x07:
						//Consider it intra also
						frame.SetIntra(true);
						//Set config
						config.SetConfigurationVersion(1);
						config.SetAVCProfileIndication(nalData[0]);
						config.SetProfileCompatibility(nalData[1]);
						config.SetAVCLevelIndication(nalData[2]);
						config.SetNALUnitLengthSizeMinus1(sizeof(nalHeader)-1);
						//Add full nal to config
						config.AddSequenceParameterSet(payload,nalSize);
						//Parse sps
						if (sps.Decode(nalData,nalSize-1))
						{
							//Set dimensions
							frame.SetWidth(sps.GetWidth());
							frame.SetHeight(sps.GetHeight());
						}
						break;
					case 0x08:
						//Consider it intra also
						frame.SetIntra(true);
						//Add full nal to config
						config.AddPictureParameterSet(payload,nalSize);
						break;
				}

				//Check if doing annex b
				if (annexB)
					//Set annex b start code
					set4(nalHeader, 0, AnnexBStartCode);
				else
					//Set size
					set4(nalHeader,0,nalSize);
				//Append data
				frame.AppendMedia(nalHeader, sizeof (nalHeader));
				
				//Append data and get current post
				pos = frame.AppendMedia(payload,nalSize);
				//Add RTP packet
				frame.AddRtpPacket(pos,nalSize,NULL,0);
				
				payload += nalSize;
				payloadLen -= nalSize;
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


			//Check length
			if (payloadLen < 2)
				return NULL;
			
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
			nalSize = payloadLen-2;

			//if it is the start fragment of the nal unit
			if (S)
			{
				/* NAL unit starts here */
				BYTE fragNalHeader = (payload[0] & 0xe0) | (payload[1] & 0x1f);

				//Get nal type
				nalUnitType = fragNalHeader & 0x1f;
				
				//Check it
				if (nalUnitType==0x05)
					//It is intra
					frame.SetIntra(true);

				//Get init of the nal
				iniFragNALU = frame.GetLength();
				//Set empty header, will be set later
				set4(nalHeader,0,0);
				//Append data
				frame.AppendMedia(nalHeader, sizeof (nalHeader));
				//Append NAL header
				frame.AppendMedia(&fragNalHeader,1);
				//We have a start frag
				startedFrag = true;
			}
			
			//If we didn't receive a start frag
			if (!startedFrag)
				//Ignore
				return NULL;

			//Append data and get current post
			pos = frame.AppendMedia(payload+2,nalSize);
			//Add rtp payload
			frame.AddRtpPacket(pos,nalSize,payload,2);

			//If it is the end fragment of the nal unit
			if (E)
			{
				//Ensure it is valid
				if (iniFragNALU+4>frame.GetLength())
					//Error
					return NULL;
				//Get NAL size
				DWORD nalSize = frame.GetLength()-iniFragNALU-4;
				//Check if doing annex b
				if (annexB)
					//Set annex b start code
					set4(frame.GetData(), iniFragNALU, AnnexBStartCode);
				else
					//Set size
					set4(frame.GetData(), iniFragNALU, nalSize);
				//Done with fragment
				iniFragNALU = 0;
				startedFrag = false;
			}
			//Done
			break;
		default:
			/* 1-23	 NAL unit	Single NAL unit packet per H.264	 5.6 */
			
			/* the entire payload is the output buffer */
			nalSize = payloadLen;
			//Check if IDR SPS or PPS
			switch (nalUnitType)
			{
				case 0x05:
					//It is intra
					frame.SetIntra(true);
					break;
				case 0x07:
					//Consider it intra also
					frame.SetIntra(true);
					//Set config
					config.SetConfigurationVersion(1);
					config.SetAVCProfileIndication(nalData[0]);
					config.SetProfileCompatibility(nalData[1]);
					config.SetAVCLevelIndication(nalData[2]);
					config.SetNALUnitLengthSizeMinus1(sizeof(nalHeader)-1);
					
					//Add full nal to config
					config.AddSequenceParameterSet(payload,nalSize);
					
					//Parse sps
					if (sps.Decode(nalData,nalSize-1))
					{
						//Set dimensions
						frame.SetWidth(sps.GetWidth());
						frame.SetHeight(sps.GetHeight());
					}
					break;
				case 0x08:
					
					//Consider it intra also
					frame.SetIntra(true);
					//Add full nal to config
					config.AddPictureParameterSet(payload,nalSize);
					break;
			}
			//Check if doing annex b
			if (annexB)
				//Set annex b start code
				set4(nalHeader, 0, AnnexBStartCode);
			else
				//Set size
				set4(nalHeader, 0, nalSize);
			//Append data
			frame.AppendMedia(nalHeader, sizeof (nalHeader));
			//Append data and get current post
			pos = frame.AppendMedia(payload, nalSize);
			//Add RTP packet
			frame.AddRtpPacket(pos,nalSize,NULL,0);
			//Done
			break;
	}

	return &frame;
}
