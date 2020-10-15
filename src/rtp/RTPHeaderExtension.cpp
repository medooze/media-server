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


size_t WriteHeaderIdAndLength(BYTE* data, DWORD pos, BYTE id, DWORD length)
{
	//Check id is valid
	if (id==RTPMap::NotFound)
		return 0;
	//Check size
	if (!length || (length-1)>0x0f)
		return 0;
	
	//Set id && length
	data[pos] = id << 4 | (length-1);
	
	//OK
	return 1;
}

/*
	https://tools.ietf.org/html/rfc5285
 
	 0			 1			 2			 3
	 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|	 0xBE    |    0xDE	 |	     length=3		|
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|  ID   | L=0   |     data	|  ID   |  L=1  |   data...
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|				  data					   |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  
*/
DWORD RTPHeaderExtension::Parse(const RTPMap &extMap,const BYTE* data,const DWORD size)
{
  	BYTE headerLength = 0;
	//If not enought size for header
	if (size<4)
		//ERROR
		return 0;
	
	//Get magic header
	WORD magic = get2(data,0);
	
	//Ensure it is magical
	if (magic==0xBEDE)
		//One byte headers
		headerLength = 1;
	else if ((magic >>4) == 0x100)
		//two byte headers
		headerLength = 2;
	else
		//ERROR
		return Error("Magic cookie not found");
	
	//Get length
	WORD length = get2(data,2)*4;
	
	//Ensure we have enought
	if (size<length+4u)
		//ERROR
		return Error("Not enought data");
  
	//Loop
	WORD i = 0;
	const BYTE* ext = data+4;
	
	//::Dump(ext,length);
  
	//Read all
	while (i<length)
	{
		BYTE len = 0;
		BYTE id  = 0;
		
		//Check header length
		if (headerLength==1)
		{
			//Get header
			const BYTE header = ext[i++];
			//If it is padding
			if (!header)
				//skip
				continue;
			//Get extension element id
			id = header >> 4;
			//Get extenion element length
			len = (header & 0x0F) + 1;
		} else {
			
			//Get extension element id
			id = ext[i++];
			
			//If it is padding
			if (!id)
				//skip
				continue;
			
			//Check size
			if (i+1>length)
				return Error("Not enought data for 2 byte header\n");;
			
			//Get extension element length
			len = ext[i++];
		}
		
		//Debug("-RTPExtension [type:%d,codec:%d,len:%d]\n",id,t,len);
		
		//   The local identifier value 15 is reserved for a future extension and
		//   MUST NOT be used as an identifier.  If the ID value 15 is
		//   encountered, its length field MUST be ignored, processing of the
		//   entire extension MUST terminate at that point, and only the extension
		//   elements present prior to the element with ID 15 SHOULD be
		//   considered.
		if (id==Reserved)
			break;
		
		//Ensure that we have enought data
		if (i+len>length)
			return Error("Not enougth data for extension\n");
		
		//Get mapped extension
		BYTE type = extMap.GetCodecForType(id);
		
		//Check type
		switch (type)
		{
			case SSRCAudioLevel:
				// The payload of the audio level header extension element can be
				// encoded using either the one-byte or two-byte 
				// 0			 1
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | len=0 |V| level	 |
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// 0			 1			 2			 3
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | len=1 |V|   level     |	0x00     |	0x00     |
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-s+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//
				// Set extennsion
				hasAudioLevel	= true;
				vad		= (ext[i] & 0x80) >> 7;
				level		= (ext[i] & 0x7f);
				break;
			case TimeOffset:
				//  0			 1			 2			 3
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | len=2 |		  transmission offset		  |
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
				//  0			 1			 2			 3
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | len=2 |		  absolute send time		   |
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
				// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
				// Set extension
				hasAbsSentTime = true;
				absSentTime = ((QWORD)get3(ext,i))*1000 >> 18;
				break;
			case CoordinationOfVideoOrientation:
				// Bit#		7   6   5   4   3   2   1  0(LSB)
				// Definition	0   0   0   0   C   F   R1 R0
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
				//  0                   1                   2                   3
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | L=1   |transport-wide sequence number | zero padding  |
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				hasTransportWideCC = true;
				transportSeqNum =  get2(ext,i);
				break;
				
			case FrameMarking:
				// For Frame Marking RTP Header Extension:
				// 
				// https://tools.ietf.org/html/draft-ietf-avtext-framemarking-04#page-4
				// This extensions provides meta-information about the RTP streams outside the
				// encrypted media payload, an RTP switch can do codec-agnostic
				// selective forwarding without decrypting the payload
				//
				// for Non-Scalable Streams
				// 
				//     0                   1
				//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
				//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//    |  ID=? |  L=0  |S|E|I|D|0 0 0 0|
				//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//
				// for Scalable Streams
				// 
				//     0                   1                   2                   3
				//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//    |  ID=? |  L=2  |S|E|I|D|B| TID |   LID         |    TL0PICIDX  |
				//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//
				//Set header
				hasFrameMarking = true;
				// Set frame marking ext
				frameMarks.startOfFrame	= ext[i] & 0x80;
				frameMarks.endOfFrame	= ext[i] & 0x40;
				frameMarks.independent	= ext[i] & 0x20;
				frameMarks.discardable	= ext[i] & 0x10;

				// Check variable length
				if (len==1) {
					// We are non-scalable
					frameMarks.baseLayerSync	= 0;
					frameMarks.temporalLayerId	= 0;
					frameMarks.layerId		= 0;
					frameMarks.tl0PicIdx		= 0;
				} else if (len==3) {
					// Set scalable parts
					frameMarks.baseLayerSync	= ext[i] & 0x08;
					frameMarks.temporalLayerId	= ext[i] & 0x07;
					frameMarks.layerId		= ext[i+1];
					frameMarks.tl0PicIdx		= ext[i+2]; 
				} else {
					// Incorrect length
					hasFrameMarking = false;
				} 
				break;
			// SDES string items
			case RTPStreamId:
				hasRId = true;
				rid.assign((const char*)ext+i,len);
				break;	
			case RepairedRTPStreamId:
				hasRepairedId = true;
				repairedId.assign((const char*)ext+i,len);
				break;	
			case MediaStreamId:
				hasMediaStreamId = true;
				mid.assign((const char*)ext+i,len);
				break;
			case DependencyDescriptor:
				//Leave it for later
				dependencyDescryptorReader.Wrap(ext+i,len);
				break;
			default:
				UltraDebug("-Unknown or unmapped extension [%d]\n",id);
				break;
		}
		//Skip length
		i += len;
	}
 
	return 4+length;
}

bool RTPHeaderExtension::ParseDependencyDescriptor(const std::optional<TemplateDependencyStructure>& templateDependencyStructure)
{
	//Check we have anything to read
	if (!dependencyDescryptorReader.Left())
		//Error
		return false;
	
	//Parse it
	dependencyDescryptor = DependencyDescriptor::Parse(dependencyDescryptorReader,templateDependencyStructure);
	//Was it parsed correctly?
	hasDependencyDescriptor = dependencyDescryptor.has_value();
	//Release reader
	dependencyDescryptorReader.Release();

	//Done
	return hasDependencyDescriptor;
}

DWORD RTPHeaderExtension::Serialize(const RTPMap &extMap,BYTE* data,const DWORD size) const
{
	size_t n;
	
	//If not enought size for header
	if (size<4)
		//ERROR
		return 0;
	
	//Set magic header
	set2(data,0,0xBEDE);
	
	//Parse and set length
	DWORD len = 4;
	
	//For each extension
	
	if (hasDependencyDescriptor && dependencyDescryptor)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(DependencyDescriptor);
		
		//Write header with dummy length
		if ((n = WriteHeaderIdAndLength(data,len,id,0)))
		{
			//Get writter
			BitWritter writter(data + len + n, size - len + n);
			
			//Serialize 
			if (dependencyDescryptor->Serialize(writter))
			{
				//Flush buffer and get lenght
				uint32_t extLen = writter.Flush();
				
				//TODO: check for two byte header extension
				
				//Rewrite header
				n = WriteHeaderIdAndLength(data,len,id,extLen);
				//Inc header len
				len += extLen + n;
			}
		}	
	}
	
	
	if (hasAudioLevel)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(SSRCAudioLevel);

		// The payload of the audio level header extension element can be
		// encoded using either the one-byte or two-byte 
		//  0			 1
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |  ID   | len=0 |V| level	   |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,1)))
		{
			//Inc header len
			len += n;
			//Set vad
			data[len++] = (vad ? 0x80 : 0x00) | (level & 0x7f);
		}
	}
	
	if (hasTimeOffset)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(TimeOffset);
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |  ID   | len=2 |		  transmission offset		   |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,3)))
		{
			//Inc header len
			len += n;
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
		BYTE id = extMap.GetTypeForCodec(AbsoluteSendTime);
		
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |  ID   | len=2 |		  absolute send time		   |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
		// Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
		//If found
		if ((n = WriteHeaderIdAndLength(data,len,id,3)))
		{
			//Inc header len
			len += n;
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
		BYTE id = extMap.GetTypeForCodec(CoordinationOfVideoOrientation);
		
		// Bit#		7   6   5   4   3   2   1  0(LSB)
		// Definition	0   0   0   0   C   F   R1 R0
		// With the following definitions:
		// C = Camera: indicates the direction of the camera used for this video stream. It can be used by the MTSI client in receiver to e.g. display the received video differently depending on the source camera.
		//     0: Front-facing camera, facing the user. If camera direction is unknown by the sending MTSI client in the terminal then this is the default value used.
		// 1: Back-facing camera, facing away from the user.
		// F = Flip: indicates a horizontal (left-right flip) mirror operation on the video as sent on the link.
		//     0: No flip operation. If the sending MTSI client in terminal does not know if a horizontal mirror operation is necessary, then this is the default value used.
		//     1: Horizontal flip operation
		// R1, R0 = Rotation: indicates the rotation of the video as transmitted on the link. The receiver should rotate the video to compensate that rotation. E.g. a 90° Counter Clockwise rotation should be compensated by the receiver with a 90° Clockwise rotation prior to displaying.
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,1)))
		{
			//Inc header len
			len += n;
			//Get all cvo data
			data[len++] = (cvo.facing ? 0x08 : 0x00) | (cvo.flip ? 0x04 : 0x00) | (cvo.rotation & 0x03);
		}
	}
	
	if (hasTransportWideCC)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(TransportWideCC);
		
		//  0                   1                   2
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |  ID   | L=1   |transport-wide sequence number | 
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,2)))
		{
			//Inc header len
			len += n;
			//Set them
			set2(data,len,transportSeqNum);
			//Inc length
			len += 2;
		}
	}
	
	if (hasFrameMarking)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(FrameMarking);
		
	
		// For Frame Marking RTP Header Extension:
		// 
		// https://tools.ietf.org/html/draft-ietf-avtext-framemarking-04#page-4
		// This extensions provides meta-information about the RTP streams outside the
		// encrypted media payload, an RTP switch can do codec-agnostic
		// selective forwarding without decrypting the payload
		//
		// for Non-Scalable Streams
		// 
		//     0			 1
		//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
		//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//    |  ID=? |  L=0  |S|E|I|D|0 0 0 0|
		//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//
		// for Scalable Streams
		// 
		//     0                   1                   2                   3
		//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//    |  ID=? |  L=2  |S|E|I|D|B| TID |   LID	   |    TL0PICIDX  |
		//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//

		bool scalable = false;
		// Check if it is scalable
		if (frameMarks.baseLayerSync || frameMarks.temporalLayerId || frameMarks.layerId || frameMarks.tl0PicIdx) 
			//It is scalable
			scalable = true;
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,(scalable ? 0x03 : 0x01))))
		{
			//Inc header len
			len += n;
			//Set common part
			data[len] = frameMarks.startOfFrame ? 0x80 : 0x00;
			data[len] |= frameMarks.endOfFrame ? 0x40 : 0x00;
			data[len] |= frameMarks.independent ? 0x20 : 0x00;
			data[len] |= frameMarks.discardable ? 0x10 : 0x00;
			
			//If it is scalable
			if (scalable)
			{
				//Set scalable data
				data[len] |= frameMarks.baseLayerSync ? 0x08 : 0x00;
				data[len] |= (frameMarks.temporalLayerId & 0x07);
				//Inc len
				len++;
				//Set other bytes
				data[len++] = frameMarks.layerId;
				data[len++] = frameMarks.tl0PicIdx;
			} else {
				//Inc len
				len++;
			}
		}
	}
	
	if (hasRId)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(RTPStreamId);
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,rid.length())))
		{
			//Inc header len
			len += n;
			//Copy str contents
			memcpy(data+len,rid.c_str(),rid.length());
			//Append length
			len+=rid.length();
		}
	}
	
	if (hasRepairedId)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(RepairedRTPStreamId);
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,repairedId.length())))
		{
			//Inc header len
			len += n;
			//Copy str contents
			memcpy(data+len,repairedId.c_str(),repairedId.length());
			//Append length
			len+=repairedId.length();
		}
	}
	
	if (hasMediaStreamId)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(MediaStreamId);
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,mid.length())))
		{
			//Inc header len
			len += n;
			//Copy str contents
			memcpy(data+len,mid.c_str(),mid.length());
			//Append length
			len+=mid.length();
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
		Debug("\t\t\t[AudioLevel vad=%d level=%d/]\n",vad,level);
	if (hasTimeOffset)
		Debug("\t\t\t[TimeOffset offset=%d/]\n",timeOffset);
	if (hasAbsSentTime)
		Debug("\t\t\t[AbsSentTime ts=%lld/]\n",absSentTime);
	if (hasVideoOrientation)
		Debug("\t\t\t[VideoOrientation facing=%d flip=%d rotation=%d/]\n",cvo.facing,cvo.flip,cvo.rotation);
	if (hasTransportWideCC)
		Debug("\t\t\t[TransportWideCC seq=%u/]\n",transportSeqNum);
	if (hasFrameMarking)
		Debug("\t\t\t[FrameMarking startOfFrame=%u endOfFrame=%u independent=%u discardable=%u baseLayerSync=%u temporalLayerId=%u layerId=%u tl0PicIdx=%u/]\n",
			frameMarks.startOfFrame,
			frameMarks.endOfFrame,
			frameMarks.independent,
			frameMarks.discardable,
			frameMarks.baseLayerSync,
			frameMarks.temporalLayerId,
			frameMarks.layerId,
			frameMarks.tl0PicIdx
		);
	
	if (hasRId)
		Debug("\t\t\t[RId str=\"%s\"]\n",rid.c_str());
	if (hasRepairedId)
		Debug("\t\t\t[RepairedId str=\"%s\"]\n",repairedId.c_str());
	if (hasMediaStreamId)
		Debug("\t\t\t[MediaStreamId str=\"%s\"]\n",mid.c_str());
	if (hasDependencyDescriptor && dependencyDescryptor)
		dependencyDescryptor->Dump();
	
	Debug("\t\t[/RTPHeaderExtension]\n");
}

