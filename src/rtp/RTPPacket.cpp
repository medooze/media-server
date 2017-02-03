/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPPacket.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:52
 */

#include "rtp.h"


void RTPPacket::ProcessExtensions(const RTPMap &extMap)
{
	//Check extensions
	if (GetX())
	{
		//Get extension data
		const BYTE* ext = GetExtensionData();
		//Get extesnion lenght
		WORD length = GetExtensionLength();
		//Read all
		while (length)
		{
			//Get header
			const BYTE header = *(ext++);
			//Decrease lenght
			length--;
			//If it is padding
			if (!header)
				//Next
				continue;
			//Get extension element id
			BYTE id = header >> 4;
			//GEt extenion element length
			BYTE n = (header & 0x0F) + 1;
			//Check consistency
			if (n>length)
				//Exit
				break;
			//Get mapped extension
			BYTE t = extMap.GetCodecForType(id);
			//Debug("-RTPExtension [type:%d,codec:%d]\n",id,t);
			//Check type
			switch (t)
			{
				case RTPHeaderExtension::SSRCAudioLevel:
					// The payload of the audio level header extension element can be
					// encoded using either the one-byte or two-byte 
					// 0                   1
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=0 |V| level       |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// 0                   1                   2                   3
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=1 |V|   level     |      0x00     |      0x00     |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-s+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					//
					// Set extennsion
					extension.hasAudioLevel = true;
					extension.vad	= (*ext & 0x80) >> 7;
					extension.level	= (*ext & 0x7f);
					break;
				case RTPHeaderExtension::TimeOffset:
					//  0                   1                   2                   3
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=2 |              transmission offset              |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					//
					// Set extension
					extension.hasTimeOffset = true;
					extension.timeOffset = get3(ext,0);
					//Check if it is negative
					if (extension.timeOffset & 0x800000)
						  // Negative offset, correct sign for Word24 to Word32.
						extension.timeOffset |= 0xFF000000;
					break;
				case RTPHeaderExtension::AbsoluteSendTime:
					//  0                   1                   2                   3
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | len=2 |              absolute send time               |
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
					// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
					// Set extension
					extension.hasAbsSentTime = true;
					extension.absSentTime = ((QWORD)get3(ext,0))*1000 >> 18;
					break;
				case RTPHeaderExtension::CoordinationOfVideoOrientation:
					// Bit#            7   6   5   4   3   2   1  0(LSB)
					// Definition      0   0   0   0   C   F   R1 R0
					// With the following definitions:
					// C = Camera: indicates the direction of the camera used for this video stream. It can be used by the MTSI client in receiver to e.g. display the received video differently depending on the source camera.
					//     0: Front-facing camera, facing the user. If camera direction is unknown by the sending MTSI client in the terminal then this is the default value used.
					// 1: Back-facing camera, facing away from the user.
					// F = Flip: indicates a horizontal (left-right flip) mirror operation on the video as sent on the link.
					//     0: No flip operation. If the sending MTSI client in terminal does not know if a horizontal mirror operation is necessary, then this is the default value used.
					//     1: Horizontal flip operation
					// R1, R0 = Rotation: indicates the rotation of the video as transmitted on the link. The receiver should rotate the video to compensate that rotation. E.g. a 90° Counter Clockwise rotation should be compensated by the receiver with a 90° Clockwise rotation prior to displaying.
					// Set extension
					extension.hasVideoOrientation = true;
					*((BYTE*)(&extension.cvo)) = *ext;
					break;
				case RTPHeaderExtension::TransportWideCC:
					//  0                   1                   2       
					//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					// |  ID   | L=1   |transport-wide sequence number | 
					// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					extension.hasTransportWideCC = true;
					extension.transportSeqNum = (WORD)get2(ext,0);
					break;
				default:
					Debug("-Unknown or unmapped extension [%d]\n",id);
					break;
			}
			//Skip bytes
			ext+=n;
			length-=n;
		}

		//Debug("-RTPExtensions [vad=%d,level=%.2d,offset=%d,ts=%lld]\n",GetVAD(),GetLevel(),GetTimeOffset(),GetAbsSendTime());
	}
}
