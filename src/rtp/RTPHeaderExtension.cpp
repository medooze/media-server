/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTPHeaderExtension.cpp
 * Author: Sergio
 * 
 * Created on 3 de febrero de 2017, 11:51
 */

#include "rtp/RTPHeaderExtension.h"
#include "log.h"

/*
	https://tools.ietf.org/html/rfc5285
 
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       0xBE    |    0xDE       |           length=3            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  ID   | L=0   |     data      |  ID   |  L=1  |   data...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          data                                 |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  
*/
DWORD RTPHeaderExtension::Parse(const RTPMap &extMap,const BYTE* data,const DWORD size)
{
	//If not enought size for header
	if (size<4)
		//ERROR
		return 0;
	
	//Get magic header
	WORD magic = get2(data,0);
	
	//Ensure it is magical
	if (magic!=0xBEDE)
		//ERROR
		return Error("Magic cookie not found");
	
	//Get length
	WORD length = get2(data,2)*4;
	
	//Ensure we have enought
	if (size<length+4)
		//ERROR
		return Error("Not enought data");
	//Loop
	WORD i = 0;
	const BYTE* ext = data+4;
	
	//Read all
	while (i<length)
	{
		//Get header
		const BYTE header = ext[i++];
		//If it is padding
		if (!header)
			//skip
			continue;
		//Get extension element id
		BYTE id = header >> 4;
		//GEt extenion element length
		BYTE n = (header & 0x0F) + 1;
		//Get mapped extension
		BYTE t = extMap.GetCodecForType(id);
		//Debug("-RTPExtension [type:%d,codec:%d]\n",id,t);
		//Check type
		switch (t)
		{
			case SSRCAudioLevel:
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
				hasAudioLevel	= true;
				vad		= (ext[i] & 0x80) >> 7;
				level		= (ext[i] & 0x7f);
				break;
			case TimeOffset:
				//  0                   1                   2                   3
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | len=2 |              transmission offset              |
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//
				// Set extension
				hasTimeOffset = true;
				timeOffset = get3(ext,i);
				//Check if it is negative
				if (timeOffset & 0x800000)
					// Negative offset
					timeOffset = -(timeOffset & 0x7FFFFF);
				break;
			case AbsoluteSendTime:
				//  0                   1                   2                   3
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | len=2 |              absolute send time               |
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
				// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
				// Set extension
				hasAbsSentTime = true;
				absSentTime = ((QWORD)get3(ext,i))*1000 >> 18;
				break;
			case CoordinationOfVideoOrientation:
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
				hasVideoOrientation = true;
				//Get all cvo data
				cvo.facing	= ext[i] >> 3;
				cvo.flip	= ext[i] >> 2 & 0x01;
				cvo.rotation	= ext[i] & 0x03;
				break;
			case TransportWideCC:
				//  0                   1                   2       
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | L=1   |transport-wide sequence number | 
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				hasTransportWideCC = true;
				transportSeqNum =  get2(ext,i);
				break;
			default:
				Debug("-Unknown or unmapped extension [%d]\n",id);
				break;
		}
		//Skip length
		i += n;
	}
	
	return 4+length;
}


DWORD RTPHeaderExtension::Serialize(const RTPMap &extMap,BYTE* data,const DWORD size) const
{
	//If not enought size for header
	if (size<4)
		//ERROR
		return 0;
	
	//Set magic header
	set2(data,0,0xBEDE);
	
	//Parse and set length
	DWORD len = 4;
	
	//For each extension
	if (hasAudioLevel)
	{
		//Get id for extension
		BYTE id = extMap.GetCodecForType(SSRCAudioLevel);
		
		//If found
		if (id)
		{
			// The payload of the audio level header extension element can be
			// encoded using either the one-byte or two-byte 
			// 0                   1
			//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			// |  ID   | len=0 |V| level       |
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			//
			//Set id && length
			data[len++] = id << 4 | 0x01;
			//Set vad
			data[len++] = (vad ? 0x80 : 0x00) | (level & 0x07);
		}
	}
	
	if (hasTimeOffset)
	{
		//Get id for extension
		BYTE id = extMap.GetCodecForType(TimeOffset);
		//If found
		if (id)
		{
							//  0                   1                   2                   3
			//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			// |  ID   | len=2 |              transmission offset              |
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			//
			//Set id && length
			data[len++] = id << 4 | 0x02;
			//if it is negative
			if (timeOffset<0)
			{
				//Set value
				set3(data,len,-timeOffset);
				//Set sign
				data[0] = data[0] | 0x80;
			} else {
				//Set value
				set3(data,len,timeOffset & 0x7FFFFF);
			}
			//Increase length
			len += 3;
		}
	}
	
	if (hasAbsSentTime)
	{
		//Get id for extension
		BYTE id = extMap.GetCodecForType(AbsoluteSendTime);
		
		//If found
		if (id)
		{
			//  0                   1                   2                   3
			//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			// |  ID   | len=2 |              absolute send time               |
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			// Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
			// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
			//Set id && length
			data[len++] = id << 4 | 0x02;
			//Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
			// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
			//Set it
			set3(data,len,((absSentTime << 18) / 1000));
			//Inc length
			len += 3;
		}
	}
		
	if (hasVideoOrientation)
	{
		//Get id for extension
		BYTE id = extMap.GetCodecForType(CoordinationOfVideoOrientation);
		
		//If found
		if (id)
		{
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
			//Set id && length
			data[len++] = id << 4 | 0x00;
			//Get all cvo data
			data[len++] = (cvo.facing ? 0x08 : 0x00) | (cvo.flip ? 0x04 : 0x00) | (cvo.rotation & 0x03);
		}
	}
	
	if (hasTransportWideCC)
	{
		//Get id for extension
		BYTE id = extMap.GetCodecForType(TransportWideCC);
		
		//If found
		if (id)
		{
		        //  0                   1                   2       
			//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			// |  ID   | L=1   |transport-wide sequence number | 
			// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			//Set id && length
			data[len++] = id << 4 | 0x01;
			//Set them
			set2(data,len,transportSeqNum);
			//Inc length
			len += 2;
		}
	}
	
	//Pad to 32 bit words
	while(len%4)
		data[len++] = 0;
	
	//Set length
	set2(data,2,(len/4)-1);
	
	//Return 
	return len;
}

void RTPHeaderExtension::Dump() const
{
	Debug("\t\t[RTPHeaderExtension]\n");
	if (hasAudioLevel)
		Debug("\t\t\t[AudioLevel vad=%d level=%d]\n",vad,level);
	if (hasTimeOffset)
		Debug("\t\t\t[TimeOffset offset=%d]\n",timeOffset);
	if (hasAbsSentTime)
		Debug("\t\t\t[AbsSentTime ts=%lld]\n",absSentTime);
	if (hasVideoOrientation)
		Debug("\t\t\t[VideoOrientation facing=%d flip=%d rotation=%d]\n",cvo.facing,cvo.flip,cvo.rotation);
	if (hasTransportWideCC)
		Debug("\t\t\t[TransportWideCC seq=%u]\n",transportSeqNum);
	Log("\t\t[/RTPHeaderExtension]\n");
}