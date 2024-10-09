 #include "rtp/RTPHeaderExtension.h"
#include "log.h"

#include <cmath>

#include "BufferReader.h"
#include "BufferWritter.h"

size_t WriteHeaderIdAndLength(BYTE* data, DWORD pos, BYTE id, DWORD length, int headerLength)
{
	//Check id is valid
	if (id==RTPMap::NotFound)
		return 0;

	switch (headerLength)
	{
		case  1:
			//Check size
			if (!length || (length-1)>0x0f)
				return Warning("-WriteHeaderIdAndLength() | Wrong length for a 1 header byte extension [len:%d]\n", length);
	
			//Set id && length
			data[pos] = id << 4 | (length-1);
	
			//OK
			return 1;
		case 2:
			//Check size
			if (length>0xff)
				return Warning("-WriteHeaderIdAndLength() | Wrong length for a 1 header byte extension [len:%d]\n", length);

			//Set id && length
			data[pos++] = id;
			data[pos] = length;

			//OK
			return 2;

		default:
			return Error("-WriteHeaderIdAndLength() | Unknown header extension size [headerLength:%d]\n", headerLength);
	}

	//Should not get here
	return 0;
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
		return Error("-RTPHeaderExtension::Parse() | Magic cookie not found\n");
	
	//Get length
	WORD length = get2(data,2)*4;
	
	//Ensure we have enought
	if (size<length+4u)
		//ERROR
		return Error("-RTPHeaderExtension::Parse() | Not enought data\n");
  
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
				return Error("-RTPHeaderExtension::Parse() | Not enought data for 2 byte header\n");;
			
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
			return Error("-RTPHeaderExtension::Parse() | Not enougth data for extension\n");
		
		//Get mapped extension
		BYTE type = extMap.GetCodecForType(id);
		
		//Check type
		switch (type)
		{
			case Type::SSRCAudioLevel:
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
			case Type::TimeOffset:
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
			case Type::AbsoluteSendTime:
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
			case Type::CoordinationOfVideoOrientation:
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
			case Type::TransportWideCC:
				//  0                   1                   2                   3
				//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				// |  ID   | L=1   |transport-wide sequence number | zero padding  |
				// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				hasTransportWideCC = true;
				transportSeqNum =  get2(ext,i);
				break;
				
			case Type::FrameMarking:
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
			case Type::RTPStreamId:
				hasRId = true;
				rid.assign((const char*)ext+i,len);
				break;	
			case Type::RepairedRTPStreamId:
				hasRepairedId = true;
				repairedId.assign((const char*)ext+i,len);
				break;	
			case Type::MediaStreamId:
				hasMediaStreamId = true;
				mid.assign((const char*)ext+i,len);
				break;
			case Type::DependencyDescriptor:
				//Leave it for later
				dependencyDescryptorReader.emplace(ext+i,len);
				break;
			case Type::AbsoluteCaptureTime:
				//	Data layout of the shortened version of abs-capture-time with a 1-byte header + 8 bytes of data:
				//
				//					0                   1                   2                   3
				//	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				//	+ -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| ID | len = 7 | absolute capture timestamp(bit 0 - 23) |
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| absolute capture timestamp(bit 24 - 55) |
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| ... (56 - 63) |
				//	+-+-+-+-+-+-+-+-+
				//	Data layout of the extended version of abs - capture - time with a 1 - byte header + 16 bytes of data :
				//
				//	0                   1                   2                   3
				//	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				//	+ -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| ID | len = 15 | absolute capture timestamp(bit 0 - 23) |
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| absolute capture timestamp(bit 24 - 55) |
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| ... (56 - 63) | estimated capture clock offset(bit 0 - 23) |
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| estimated capture clock offset(bit 24 - 55) |
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	| ... (56 - 63) |
				//	+-+-+-+-+-+-+-+-+
				// 
				//	
				//	Absolute capture timestamp
				//	Absolute capture timestamp is the NTP timestamp of when the first frame in a packet was originally captured.
				//	This timestamp MUST be based on the same clock as the clock used to generate NTP timestamps for RTCP sender reports on the capture system.
				//
				//	This field is encoded as a 64 - bit unsigned fixed - point number with the high 32 bits for the timestamp in secondsand low 32 bits for the fractional part.
				//	This is also known as the UQ32.32 format and is what the RTP specification defines as the canonical format to represent NTP timestamps.
				//
				//	Estimated capture clock offset
				//	Estimated capture clock offset is the sender‘s estimate of the offset between its own NTP clock and the capture system’s NTP clock.
				//	The sender is here defined as the system that owns the NTP clock used to generate the NTP timestamps for the RTCP sender reports on this stream.
				//	The sender system is typically either the capture system or a mixer.
				//
				//	This field is encoded as a 64 - bit two’s complement signed fixed - point number with the high 32 bits for the secondsand low 32 bits for the fractional part.
				//	It’s intended to make it easy for a receiver, that knows how to estimate the sender system’s NTP clock, to also estimate the capture system’s NTP clock :
				// 
				//	  Capture NTP Clock = Sender NTP Clock + Capture Clock Offset
				hasAbsoluteCaptureTime = true;
				absoluteCaptureTime.absoluteCatpureTimestampNTP = get8(ext,i);
				if (len==16)
					absoluteCaptureTime.estimatedCaptureClockOffsetNTP = (int64_t )get8(ext, i+8);
				break;
			case Type::PlayoutDelay:
			{
				//
				//	 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	|  ID   | len=2 |       MIN delay       |       MAX delay       |
				//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	12 bits for Minimum and Maximum delay. 
				//	This represents a range of 0 - 40950 milliseconds for minimum and maximum (with a granularity of 10 ms).
				hasPlayoutDelay = true;
				//Get extension data
				uint32_t raw = get3(ext, i);
				//Get min & max
				playoutDelay.min = (raw >> 12) * PlayoutDelay::GranularityMs;
				playoutDelay.max = (raw & 0xfff) * PlayoutDelay::GranularityMs;
				break;
			}
			case Type::ColorSpace:
			{
				//
				// Data layout without HDR metadata (one-byte RTP header extension) 1-byte header + 4 bytes of data:
				//
				//	  0                   1                   2                   3
				//	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |  ID   | L = 3 |   primaries   |   transfer    |    matrix     |
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |range+chr.sit. |
				//	 +-+-+-+-+-+-+-+-+
				//
				// Data layout of color space with HDR metadata (two-byte RTP header extension) 2-byte header + 28 bytes of data:
				//
				//	  0                   1                   2                   3
				//	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |      ID       |   length=27   |   primaries   |   transfer    |
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |    matrix     |range+chr.sit. |         luminance_max         |
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |         luminance_min         |            mastering_metadata.|
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |primary_r.x and .y             |            mastering_metadata.|
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |primary_g.x and .y             |            mastering_metadata.|
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |primary_b.x and .y             |            mastering_metadata.|
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 |white.x and .y                 |    max_content_light_level    |
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//	 | max_frame_average_light_level |
				//	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				//
				if (len!=4 && len!=28)
					break;
				//Init flag and optional data
				hasColorSpace = true;
				colorSpace.emplace();

				//Get reader
				BufferReader reader(ext+i, len);

				//Get base config
				colorSpace->primaries	 = reader.Get1();
				colorSpace->transfer	 = reader.Get1();
				colorSpace->matrix	 = reader.Get1();
				BYTE rangeChromaSiting	 = reader.Get1();

				//Range and chroma siting : (range << 4) + (horz << 2) + vert.
				colorSpace->range			= rangeChromaSiting >> 4;
				colorSpace->chromeSitingHorizontal	= (rangeChromaSiting >> 2 ) & 0b11;
				colorSpace->chromeSitingVertical	= rangeChromaSiting & 0b11;

				//Check if we have hdr metadata
				if (len == 28)
				{
					//Init data
					colorSpace->hdrMetadata.emplace();

					//Luminance
					colorSpace->hdrMetadata->luminanceMax	= reader.Get1();
					colorSpace->hdrMetadata->luminanceMin	= reader.Get1();
					//Red
					BYTE primaryR = reader.Get1();
					colorSpace->hdrMetadata->primaryRX	= primaryR >>4;
					colorSpace->hdrMetadata->primaryRY	= primaryR & 0b1111;
					//Green
					BYTE primaryG = reader.Get1();
					colorSpace->hdrMetadata->primaryGX	= primaryG >> 4;
					colorSpace->hdrMetadata->primaryGY	= primaryG & 0b1111;
					//Blue
					BYTE primaryB = reader.Get1();
					colorSpace->hdrMetadata->primaryBX	= primaryB >> 4;
					colorSpace->hdrMetadata->primaryBY	= primaryB & 0b1111;
					//White
					BYTE white = reader.Get1();
					colorSpace->hdrMetadata->whiteX		= white >> 4;
					colorSpace->hdrMetadata->whiteY		= white & 0b1111;
					//Light
					colorSpace->hdrMetadata->maxContentLightLevel		= reader.Get2();
					colorSpace->hdrMetadata->maxFrameAverageLightLevel	= reader.Get2();
				}

				break;
			}
			case Type::VideoLayersAllocation:
			{
				
				//Get reader for extension data
				BufferReader reader(ext + i, len);

				//Init data, flag will be set when parsing is ok
				videoLayersAllocation = VideoLayersAllocation::Parse(reader);
				
				//Check if correctly parsed
				hasVideoLayersAllocation = videoLayersAllocation.has_value();

				break;
			}
			default:
				UltraDebug("-RTPHeaderExtension::Parse() | Unknown or unmapped extension [%d]\n",id);
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
	if (!dependencyDescryptorReader)
		//Error
		return false;
	
	//Parse it
	BitReader bitrader(*dependencyDescryptorReader);
	dependencyDescryptor = DependencyDescriptor::Parse(bitrader,templateDependencyStructure);
	//Was it parsed correctly?
	hasDependencyDescriptor = dependencyDescryptor.has_value();
	//Release reader
	dependencyDescryptorReader.reset();

	//Done
	return hasDependencyDescriptor;
}

DWORD RTPHeaderExtension::Serialize(const RTPMap &extMap,BYTE* data,const DWORD size) const
{
	size_t n;
	
	//If not enought size for header
	if (size<4)
		//ERROR
		return Warning("-RTPHeaderExtension::Serialize() | Not enought size [size:%d]\n", size);

	//Try with 1 byte header length first
	int headerLength = 1;

	//Check if any of the ids is too big
	if (hasRId && rid.length() > 0x0f)
		headerLength = 2;
	else if (hasRepairedId && repairedId.length() > 0x0f)
		headerLength = 2;
	else if (hasMediaStreamId && mid.length() > 0x0f)
		headerLength = 2;
			

	//Write the header later
	DWORD len = 4;
	
	//First dependency descriptor to make sure it fits 
	if (hasDependencyDescriptor && dependencyDescryptor)
	{
		//Use a temporary memory to serialize and check final size
		BYTE ext[255];

		//Get writter
		BitWriter writter(ext, sizeof(ext));

		//Serialize 
		if (dependencyDescryptor->Serialize(writter))
		{
			//Flush buffer and get lenght
			uint32_t extLen = writter.Flush();

			//Check length
			if (extLen>0x0f)
				//We need to use 2 byte header extensions
				headerLength = 2;

			//Get id for extension
			//Get id for extension
			BYTE id = extMap.GetTypeForCodec(DependencyDescriptor);

			//Write header 
			if ((n = WriteHeaderIdAndLength(data,len,id,extLen,headerLength)))
			{
				//Inc header len
				len += n;
				//Copy str contents
				memcpy(data + len, ext, extLen);
				//Append length
				len += extLen;
			}
		}
	}

	if (hasVideoLayersAllocation && videoLayersAllocation)
	{
		//Use a temporary memory to serialize and check final size
		BYTE ext[255];

		BufferWritter writer(ext,255);

		if (videoLayersAllocation->Serialize(writer))
		{
			//Get writted length
			uint32_t extLen = writer.GetLength();

			//Check length
			if (extLen > 0x0f)
				//We need to use 2 byte header extensions
				headerLength = 2;
		
			//Get id for extension
			BYTE id = extMap.GetTypeForCodec(VideoLayersAllocation);

			//Write header 
			if ((n = WriteHeaderIdAndLength(data, len, id, extLen, headerLength)))
			{
				//Inc header len
				len += n;
				//Copy str contents
				memcpy(data + len, ext, extLen);
				//Append length
				len += extLen;
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
		if ((n = WriteHeaderIdAndLength(data,len,id,1,headerLength)))
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
		if ((n = WriteHeaderIdAndLength(data,len,id,3,headerLength)))
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
		if ((n = WriteHeaderIdAndLength(data,len,id,3,headerLength)))
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
		if ((n = WriteHeaderIdAndLength(data,len,id,1,headerLength)))
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
		if ((n = WriteHeaderIdAndLength(data,len,id,2,headerLength)))
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
		//    |  ID=? |  L=2  |S|E|I|D|B| TID |   LID         |    TL0PICIDX  |
		//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//

		bool scalable = false;
		// Check if it is scalable
		if (frameMarks.baseLayerSync || frameMarks.temporalLayerId || frameMarks.layerId || frameMarks.tl0PicIdx) 
			//It is scalable
			scalable = true;
		
		//Write header 
		if ((n = WriteHeaderIdAndLength(data,len,id,(scalable ? 3 : 1),headerLength)))
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
		if ((n = WriteHeaderIdAndLength(data,len,id,rid.length(),headerLength)))
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
		if ((n = WriteHeaderIdAndLength(data,len,id,repairedId.length(),headerLength)))
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
		if ((n = WriteHeaderIdAndLength(data,len,id,mid.length(),headerLength)))
		{
			//Inc header len
			len += n;
			//Copy str contents
			memcpy(data+len,mid.c_str(),mid.length());
			//Append length
			len+=mid.length();
		}
	}
	
	if (hasAbsoluteCaptureTime)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(AbsoluteCaptureTime);

		//Check length
		DWORD extLength = absoluteCaptureTime.estimatedCaptureClockOffsetNTP ? 16 : 8;

		//Write header 
		if ((n = WriteHeaderIdAndLength(data, len, id, extLength, headerLength)))
		{
			//Inc header len
			len += n;
			//Set ntp timestamp
			set8(data,len, absoluteCaptureTime.absoluteCatpureTimestampNTP);
			//Increase len
			len+=8;
			//If we have offset toot
			if (absoluteCaptureTime.estimatedCaptureClockOffsetNTP)
			{
				//Set ntp offset
				set8(data, len, absoluteCaptureTime.estimatedCaptureClockOffsetNTP);
				//Increase len
				len += 8;
			}
		}
	}

	if (hasPlayoutDelay)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(PlayoutDelay);

		//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//	|  ID   | len=2 |       MIN delay       |       MAX delay       |
		//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

		//Write header 
		if ((n = WriteHeaderIdAndLength(data, len, id, 3, headerLength)))
		{
			//Inc header len
			len += n;

			//Set them
			set3(data, len , ((playoutDelay.min / PlayoutDelay::GranularityMs) << 12) | ((playoutDelay.max / PlayoutDelay::GranularityMs ) & 0xfff));

			//Inc length
			len += 3;
		}

	}

	if (hasColorSpace && colorSpace)
	{
		//Get id for extension
		BYTE id = extMap.GetTypeForCodec(ColorSpace);

		//Write header 
		if ((n = WriteHeaderIdAndLength(data, len, id, (colorSpace->hdrMetadata ? 28 : 4), headerLength)))
		{
			//Inc header len
			len += n;

			//Get base config
			data[len++] = colorSpace->primaries;
			data[len++] = colorSpace->transfer;
			data[len++] = colorSpace->matrix;
			data[len++] = (colorSpace->range << 4 )
				| (colorSpace->chromeSitingHorizontal & 0b11 << 2 )
				| (colorSpace->chromeSitingVertical & 0b11);

			//Check if we have hdr metadata
			if (colorSpace->hdrMetadata)
			{
				//Luminance
				data[len++] = colorSpace->hdrMetadata->luminanceMax;
				data[len++] = colorSpace->hdrMetadata->luminanceMin;
				//Red
				data[len++] = (colorSpace->hdrMetadata->primaryRX << 4 )
					| (colorSpace->hdrMetadata->primaryRY & 0b1111);
				//Green
				data[len++] = (colorSpace->hdrMetadata->primaryGX << 4)
					| (colorSpace->hdrMetadata->primaryGY & 0b1111);
				//Blue
				data[len++] = (colorSpace->hdrMetadata->primaryBX << 4)
					| (colorSpace->hdrMetadata->primaryBX & 0b1111);
				//White
				data[len++] = (colorSpace->hdrMetadata->whiteX << 4)
					| (colorSpace->hdrMetadata->whiteY & 0b1111);
				//Light
				set2(data, len, colorSpace->hdrMetadata->maxContentLightLevel);
				len +=2;
				set2(data, len, colorSpace->hdrMetadata->maxFrameAverageLightLevel);
				len += 2;
			}
		}
	}


	//Pad to 32 bit words
	while(len%4)
		data[len++] = 0;

	//Set magic header
	if (headerLength==1)
		set2(data, 0, 0xBEDE);
	else 
		set2(data, 0, 0x1000);
	
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
	if (hasAbsoluteCaptureTime)
		Debug("\t\t\t[AbsoluteCaptureTime absoluteCatpureTimestampNTP=%llu estimatedCaptureClockOffsetNTP=%lld absoluteCaptureTimestamp=%llu absoluteCaptureTime=%llu/]\n",
			absoluteCaptureTime.absoluteCatpureTimestampNTP,
			absoluteCaptureTime.estimatedCaptureClockOffsetNTP,
			absoluteCaptureTime.GetAbsoluteCaptureTimestamp(),
			absoluteCaptureTime.GetAbsoluteCaptureTime()
		);
	if (hasPlayoutDelay)
		Debug("\t\t\t[PlayoutDelay min=%u max=%u]\n", playoutDelay.min, playoutDelay.max);
	if (hasColorSpace && colorSpace)
	{
		Debug("\t\t\t[ColorSpace primaries=%u transfer=%u matrix=%u range=%u chromeSitingHorizontal=%u chromeSitingVertical=%u/]\n",
			colorSpace->primaries,
			colorSpace->transfer,
			colorSpace->matrix,
			colorSpace->range,
			colorSpace->chromeSitingHorizontal,
			colorSpace->chromeSitingVertical
		);
		if (colorSpace->hdrMetadata)
			Debug("\t\t\t[ColorSpace HDR Metadata luminanceMax=%u luminanceMin=%u primaryRX=%u primaryRY=%u primaryGX=%u primaryGY=%u primaryBX=%u primaryBY=%u  whiteX=%u whiteY=%u  maxContentLightLevel=%u maxFrameAverageLightLevel=%u/]\n",
				colorSpace->hdrMetadata->luminanceMax,
				colorSpace->hdrMetadata->luminanceMin,
				colorSpace->hdrMetadata->primaryRX,
				colorSpace->hdrMetadata->primaryRY,
				colorSpace->hdrMetadata->primaryGX,
				colorSpace->hdrMetadata->primaryGY,
				colorSpace->hdrMetadata->primaryBX,
				colorSpace->hdrMetadata->primaryBY,
				colorSpace->hdrMetadata->whiteX,
				colorSpace->hdrMetadata->whiteY,
				colorSpace->hdrMetadata->maxContentLightLevel,
				colorSpace->hdrMetadata->maxFrameAverageLightLevel
			);
	}

	Debug("\t\t[/RTPHeaderExtension]\n");
}

