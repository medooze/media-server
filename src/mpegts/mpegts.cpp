#include "mpegts/mpegts.h"
#include "log.h"
#include "bitstream/BitReader.h"
#include "bitstream/BitWriter.h"

namespace mpegts
{

void Header::Encode(BufferWritter& writer) const
{
	if (writer.GetLeft() < Size())
		throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
		
	BitWriter bitwriter(writer, 4);
	bitwriter.Put(8, syncByte);
	bitwriter.Put(1, transportErrorIndication);
	bitwriter.Put(1, payloadUnitStartIndication);
	bitwriter.Put(1, transportPriority);
	bitwriter.Put(13, packetIdentifier);
	bitwriter.Put(2, transportScramblingControl);
	bitwriter.Put(2, adaptationFieldControl);
	bitwriter.Put(4, continuityCounter);
}

size_t Header::Size() const
{
	return 4;
}

Header Header::Parse(BufferReader& reader)
{
	if (reader.GetLeft()<4)
		throw std::runtime_error("Not enought data to read mpegts packet header");

	//Get the MPEG TS header bytes
	BitReader bitreader(reader);

	//The hceader
	Header header = {};

	/*
	Sync Byte				8	0xff000000	Bit pattern of 0x47 (ASCII char 'G')
	Transport error indicator (TEI)		1	0x800000	Set when a demodulator can't correct errors from FEC data; indicating the packet is corrupt.[9]
	Payload unit start indicator (PUSI)	1	0x400000	Set when this packet contains the first uint8_t of a new payload unit. The first uint8_t of the payload will indicate where this new payload unit starts.
									This field allows a receiver that started reading mid transmission to know when it can start extracting data.
	Transport priority			1	0x200000	Set when the current packet has a higher priority than other packets with the same PID.
	PID					13	0x1fff00	Packet Identifier, describing the payload data.
	Transport scrambling control (TSC)	2	0xc0		'00' = Not scrambled.
									For DVB-CSA and ATSC DES only:[10]
									'01' (0x40) = Reserved for future use
									'10' (0x80) = Scrambled with even key
									'11' (0xC0) = Scrambled with odd key

	Adaptation field control		2	0x30		01 - no adaptation field, payload only,
									10 - adaptation field only, no payload,
									11 - adaptation field followed by payload,
									00 - RESERVED for future use [11]

	Continuity counter			4	0xf		Sequence number of payload packets (0x00 to 0x0F) within each stream (except PID 8191)
									Incremented per-PID, only when a payload flag is set.
	*/
	header.syncByte				= bitreader.Get(8);
	header.transportErrorIndication		= bitreader.Get(1);
	header.payloadUnitStartIndication	= bitreader.Get(1);
	header.transportPriority		= bitreader.Get(1);
	header.packetIdentifier			= bitreader.Get(13);
	header.transportScramblingControl	= bitreader.Get(2);
	header.adaptationFieldControl		= (AdaptationFieldControl)bitreader.Get(2);
	header.continuityCounter		= bitreader.Get(4);

	//Done reading
	bitreader.Flush();

	//Done
	return header;
}

void Header::Dump() const
{
	//Dump
	Debug("[Header sync=%x,tei=%d,pusi=%d,prio=%d,pid=%d,tsc=%d,afc=%d,cc=%d/]\n",
		syncByte,
		transportErrorIndication,
		payloadUnitStartIndication,
		transportPriority,
		packetIdentifier,
		transportScramblingControl,
		adaptationFieldControl,
		continuityCounter
	);
}


void AdaptationField::Encode(BufferWritter& writer) const
{
	if (writer.GetLeft() < Size())
		throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
	
	if (opcrFlag || splicingPointFlag || transportPrivateDataFlag || adaptationFieldExtensionFlag)
	{
		throw std::runtime_error("The field is not implemented for encoding");
	}
	
	writer.Set1(Size() - 1);
	
	BitWriter bitwriter(writer, 1);
	
	bitwriter.Put(1, discontinuityIndicator);
	bitwriter.Put(1, randomAccessIndicator);
	bitwriter.Put(1, elementaryStreamPriorityIndicator);
	bitwriter.Put(1, pcrFlag);
	bitwriter.Put(1, opcrFlag);
	bitwriter.Put(1, splicingPointFlag);
	bitwriter.Put(1, transportPrivateDataFlag);
	bitwriter.Put(1, adaptationFieldExtensionFlag);
	
	if (pcr)
	{
		assert(pcrFlag);
		writer.Set1(uint8_t(*pcr >> 25));
		writer.Set1(uint8_t(*pcr >> 17));
		writer.Set1(uint8_t(*pcr >> 9));
		writer.Set1(uint8_t(*pcr >> 1));
		writer.Set1(uint8_t(*pcr << 7 | 0x7e));
		writer.Set1(0);
	}
}

size_t AdaptationField::Size() const
{
	return 1 + 1 + (pcr ? 6 : 0);
}

AdaptationField AdaptationField::Parse(BufferReader& reader)
{
	if (reader.GetLeft() < 1)
		throw std::runtime_error("Not enought data to read mpegts adaptation field length");

	/*
			Adaptation field length			8		Number of uint8_ts in the adaptation field immediately following this uint8_t
			Discontinuity indicator			1	0x80	Set if current TS packet is in a discontinuity state with respect to either the continuity counter or the program clock reference
			Random access indicator			1	0x40	Set when the stream may be decoded without errors from this point
			Elementary stream priority indicator	1	0x20	Set when this stream should be considered "high priority"
			PCR flag				1	0x10	Set when PCR field is present
			OPCR flag				1	0x08	Set when OPCR field is present
			Splicing point flag			1	0x04	Set when splice countdown field is present
			Transport private data flag		1	0x02	Set when transport private data is present
			Adaptation field extension flag		1	0x01	Set when adaptation extension data is present
	*/
	uint8_t adaptationFieldLength = reader.Get1();

	//Ensure we have enough
	if (adaptationFieldLength > reader.GetLeft())
		throw std::runtime_error("Not enought data to read mpegts adaptation field");

	//Get current position
	uint32_t start = reader.Mark();

	AdaptationField adaptationField = {};

	//Get bit reader
	BitReader bitreader(reader);

	adaptationField.discontinuityIndicator			= bitreader.Get(1);
	adaptationField.randomAccessIndicator			= bitreader.Get(1);
	adaptationField.elementaryStreamPriorityIndicator	= bitreader.Get(1);
	adaptationField.pcrFlag					= bitreader.Get(1);
	adaptationField.opcrFlag				= bitreader.Get(1);
	adaptationField.splicingPointFlag			= bitreader.Get(1);
	adaptationField.transportPrivateDataFlag		= bitreader.Get(1);
	adaptationField.adaptationFieldExtensionFlag		= bitreader.Get(1);

	//Done reading
	bitreader.Flush();

	//Go to the end of the adaptation field
	reader.GoTo(start + adaptationFieldLength);

	//Done
	return adaptationField;
}

Packet Packet::Parse(BufferReader& reader)
{

	//Get packet header
	Packet packet =  { mpegts::Header::Parse(reader)};

	//Check if it has adaptation field
	if (packet.header.adaptationFieldControl == AdaptationFieldOnly || packet.header.adaptationFieldControl == AdaptationFiedlAndPayload)
		//Parse Adaptation field
		packet.adaptationField = mpegts::AdaptationField::Parse(reader);

	//If it has payload
	if (packet.header.adaptationFieldControl == PayloadOnly || packet.header.adaptationFieldControl == AdaptationFiedlAndPayload)
	{
		/*
			Payload Pointer (optional)	8	0xff	Present only if the Payload Unit Start Indicator (PUSI) flag is set.
									It gives the index after this uint8_t at which the new payload unit starts.
									Any payload uint8_t before the index is part of the previous payload unit.
			Actual Payload	variable			The content of the payload.
		*/
		packet.payloadPointer = 0;

		//If we have a new payload unit
		if (packet.header.payloadUnitStartIndication)
		{
			//Ensure we have at least 1 uint8_t
			if (!reader.GetLeft())
				throw std::runtime_error("Not enought data to read mpegts payload pointer");

			//Get optional payload pointer
			packet.payloadPointer = reader.Peek1();

			//If we have any
			if (*packet.payloadPointer)
				//Consume it
				reader.Skip(1);
		}
	}

	return packet;
}

namespace pes
{

void Header::Encode(BufferWritter& writer) const
{
	if (writer.GetLeft() < Size())
		throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
	
	writer.Set3(packetStartCodePrefix);
	writer.Set1(streamId);
	writer.Set2(packetLength);
}

size_t Header::Size() const
{
	return 6;
}


Header Header::Parse(BufferReader& reader)
{
	if (reader.GetLeft() < 6)
		throw std::runtime_error("Not enought data to read mpegtsp pes header");

	/* PES packet header
	Packet start code prefix	3 uint8_ts	0x000001
	Stream id			1 uint8_t	Examples: Audio streams (0xC0-0xDF), Video streams (0xE0-0xEF) [4][5]
	Note: The above 4 uint8_ts is called the 32 bit start code.
	PES Packet length		2 uint8_ts	Specifies the number of uint8_ts remaining in the packet after this field. Can be zero. If the PES packet length is set to zero, the PES packet can be of any length. A value of zero for the PES packet length can be used only when the PES packet payload is a video elementary stream.[6]
	Optional PES header		variable length (length >= 3)	not present in case of Padding stream & Private stream 2 (navigation data)
	*/
	Header header = {};

	header.packetStartCodePrefix = reader.Get3();
	header.streamId = reader.Get1();
	header.packetLength = reader.Get2();

	return header;
}

void HeaderExtension::Encode(BufferWritter& writer) const
{
	if (writer.GetLeft() < Size())
		throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
		
	if (escrFlag || rateFlag || trickModeFlag || aditionalInfoFlag || crcFlag || extensionFlag)
		throw std::runtime_error("The field is not implemented for encoding");
	
	BitWriter bitwriter(writer, 3);
	
	bitwriter.Put(2, markerBits);
	bitwriter.Put(2, scramblingControl);
	bitwriter.Put(1, priority);
	bitwriter.Put(1, dataAlignmentIndicator);
	bitwriter.Put(1, copyright);
	bitwriter.Put(1, original);
	bitwriter.Put(2, ptsdtsIndicator);
	bitwriter.Put(1, escrFlag);
	bitwriter.Put(1, rateFlag);
	bitwriter.Put(1, trickModeFlag);
	bitwriter.Put(1, aditionalInfoFlag);
	bitwriter.Put(1, crcFlag);
	bitwriter.Put(1, extensionFlag);
	bitwriter.Put(8, Size() - 3);
	
	assert(ptsdtsIndicator != PTSDTSIndicator::Forbiden);
	
	if (ptsdtsIndicator == PTSDTSIndicator::Both)
	{
		assert(pts && dts);
		
		{
			BitWriter bitwriter(writer, 5);
			bitwriter.Put(4, 0x3);
			bitwriter.Put(3, (*pts >> 30) & 0x7);
			bitwriter.Put(1, 0x1);
			bitwriter.Put(15, (*pts >> 15) & 0x7fff);
			bitwriter.Put(1, 0x1);
			bitwriter.Put(15, (*pts) & 0x7fff);
			bitwriter.Put(1, 0x1);
		}
		
		{
			BitWriter bitwriter(writer, 5);
			bitwriter.Put(4, 0x1);
			bitwriter.Put(3, (*dts >> 30) & 0x7);
			bitwriter.Put(1, 0x1);
			bitwriter.Put(15, (*dts >> 15) & 0x7fff);
			bitwriter.Put(1, 0x1);
			bitwriter.Put(15, (*dts) & 0x7fff);
			bitwriter.Put(1, 0x1);
		}
	}
	else if (ptsdtsIndicator == PTSDTSIndicator::OnlyPTS)
	{
		BitWriter bitwriter(writer, 5);
		bitwriter.Put(4, 0x2);
		bitwriter.Put(3, (*pts >> 30) & 0x7);
		bitwriter.Put(1, 0x1);
		bitwriter.Put(15, (*pts >> 15) & 0x7fff);
		bitwriter.Put(1, 0x1);
		bitwriter.Put(15, (*pts) & 0x7fff);
		bitwriter.Put(1, 0x1);
	}
	
	writer.PadTo(writer.GetLength() + stuffingCount, 0xff);
}

size_t HeaderExtension::Size() const
{
	return 3 + (pts ? 5 : 0) + (dts ? 5 : 0) + stuffingCount;
}

HeaderExtension HeaderExtension::Parse(BufferReader& reader)
{
	if (reader.GetLeft() < 3)
		throw std::runtime_error("Not enought data to read mpegtsp pes extension header");

	/*
	Marker bits			2	10 binary or 0x2 hex
	Scrambling control		2	00 implies not scrambled
	Priority			1
	Data alignment indicator	1	1 indicates that the PES packet header is immediately followed by the video start code or audio syncword
	Copyright			1	1 implies copyrighted
	Original or Copy		1	1 implies original
	PTS DTS indicator		2	11 = both present, 01 is forbidden, 10 = only PTS, 00 = no PTS or DTS
	ESCR flag			1
	ES rate flag			1
	DSM trick mode flag		1
	Additional copy info flag	1
	CRC flag			1
	extension flag			1
	PES header length		8	gives the length of the remainder of the PES header in uint8_ts
	Optional fields	variable length	presence is determined by flag bits above
	Stuffing uint8_ts	variable length	0xff
	*/
	//Get extended header
	BitReader bitreader(reader);

	//Parse packet header
	HeaderExtension headerExtension = {};

	headerExtension.markerBits		= bitreader.Get(2);
	headerExtension.scramblingControl	= bitreader.Get(2);
	headerExtension.priority		= bitreader.Get(1);
	headerExtension.dataAlignmentIndicator	= bitreader.Get(1);
	headerExtension.copyright		= bitreader.Get(1);
	headerExtension.original		= bitreader.Get(1);
	headerExtension.ptsdtsIndicator		= (PTSDTSIndicator)bitreader.Get(2);
	headerExtension.escrFlag		= bitreader.Get(1);
	headerExtension.rateFlag		= bitreader.Get(1);
	headerExtension.trickModeFlag		= bitreader.Get(1);
	headerExtension.aditionalInfoFlag	= bitreader.Get(1);
	headerExtension.crcFlag			= bitreader.Get(1);
	headerExtension.extensionFlag		= bitreader.Get(1);
	auto remainderHeaderLength		= bitreader.Get(8);

	//Done reading
	bitreader.Flush();

	//Get current postition
	auto pos = reader.Mark();

	//Read PTS
	if (headerExtension.ptsdtsIndicator == Both || headerExtension.ptsdtsIndicator == OnlyPTS)
	{
		if (reader.GetLeft() < 5)
			throw std::runtime_error("Not enought data to read mpegtsp pes extension header PTS");

		//PTS is 33 bits
		BitReader ptsreader(reader);
		//Read parts
		ptsreader.Skip(4);
		uint64_t pts32_30 = ptsreader.Get(3);
		ptsreader.Skip(1);
		uint64_t pts29_15 = ptsreader.Get(15);
		ptsreader.Skip(1);
		uint64_t pts14_0 = ptsreader.Get(15);
		//Get PTS
		headerExtension.pts = (pts32_30 << 30) | (pts29_15 << 15) | pts14_0;

		//Done reading
		bitreader.Flush();
	}

	//Read DTS
	if (headerExtension.ptsdtsIndicator == Both)
	{
		if (reader.GetLeft() < 5)
			throw std::runtime_error("Not enought data to read mpegtsp pes extension header DTS");

		//PTS is 33 bits
		BitReader ptsreader(reader);
		//Read parts
		ptsreader.Skip(4);
		uint64_t dts32_30 = ptsreader.Get(3);
		ptsreader.Skip(1);
		uint64_t dts29_15 = ptsreader.Get(15);
		ptsreader.Skip(1);
		uint64_t dts14_0 = ptsreader.Get(15);
		//Get PTS
		headerExtension.dts = (dts32_30 << 30) | (dts29_15 << 15) | dts14_0;

		//Done reading
		bitreader.Flush();
	}

	//Skip the rest of the header
	reader.GoTo(pos + remainderHeaderLength);

	return headerExtension;
}

/*
	Stream ID		Stream type					extension present?
	1011 1101 0xBD		Private stream 1 (non MPEG audio, subpictures)	Yes
	1011 1110 0xBE		Padding stream					No
	1011 1111 0xBF		Private stream 2 (navigation data)		No
	110x xxxx 0xC0 - 0xDF	MPEG-1 or MPEG-2 audio stream number x xxxx	Yes
	1110 xxxx 0xE0 - 0xEF	MPEG-1 or MPEG-2 video stream number xxxx	Yes
*/
StreamType GetStreamType(const uint8_t& streamId)
{
	if (streamId == 0xBD)
		return PrivateStream1;
	else if (streamId == 0xBE)
		return PaddingStream;
	else if (streamId == 0xBF)
		return PrivateStream2;
	else if (streamId >= 0xC0 && streamId <= 0xDF)
		return AudioStream;
	else if (streamId >= 0xE0 && streamId <= 0xEF)
		return VideoStream;
	else
		return Invalid;
}

Packet Packet::Parse(BufferReader& reader)
{
	//Parse header
	Packet packet = {Header::Parse(reader)};

	//Check stream type
	switch (mpegts::pes::GetStreamType(packet.header.streamId))
	{
		case PrivateStream1:
			[[fallthrough]];
		case AudioStream:
			[[fallthrough]];
		case VideoStream:
			//parse packet extesion
			packet.headerExtension = HeaderExtension::Parse(reader);
			break;
		default:
			//Do nothing
			;
	}

	//Done
	return packet;
}

namespace adts
{
	void Header::Encode(BufferWritter& writer) const
	{
		if (writer.GetLeft() < Size())
			throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
		
		BitWriter bitwriter(writer, Size());
		
		bitwriter.Put(12, syncWord);
		bitwriter.Put(1, version);
		bitwriter.Put(2, layer);
		bitwriter.Put(1, protectionAbsence);
		bitwriter.Put(2, objectTypeMinus1);
		bitwriter.Put(4, samplingFrequency);
		bitwriter.Put(1, priv);
		bitwriter.Put(3, channelConfiguration);
		bitwriter.Put(1, originality);
		bitwriter.Put(1, home);
		bitwriter.Put(1, copyright);
		bitwriter.Put(1, copyrightStart);
		bitwriter.Put(13, frameLength);
		bitwriter.Put(11, bufferFullness);
		bitwriter.Put(2, numberOfFramesMinus1);
		
		if (!protectionAbsence)
			bitwriter.Put(16, crc);
	}
	
	size_t Header::Size() const
	{
		return 7 + (protectionAbsence ? 0 : 2);
	}

	Header Header::Parse(BufferReader& reader)
	{
		//Ensure we have enought data for the header
		if (reader.GetLeft() < 7)
			throw std::runtime_error("Not enought data to read mpegtsp pes adts header");

	/*	ADTS header
		Letter	Length (bits)	Description
		A	12	Syncword, all bits must be set to 1.
		B	1	MPEG Version, set to 0 for MPEG-4 and 1 for MPEG-2.
		C	2	Layer, always set to 0.
		D	1	Protection absence, set to 1 if there is no CRC and 0 if there is CRC.
		E	2	Profile, the MPEG-4 Audio Object Type minus 1.
		F	4	MPEG-4 Sampling Frequency Index (15 is forbidden).
		G	1	Private bit, guaranteed never to be used by MPEG, set to 0 when encoding, ignore when decoding.
		H	3	MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE (Program Config Element)).
		I	1	Originality, set to 1 to signal originality of the audio and 0 otherwise.
		J	1	Home, set to 1 to signal home usage of the audio and 0 otherwise.
		K	1	Copyright ID bit, the next bit of a centrally registered copyright identifier. This is transmitted by sliding over the bit-string in LSB-first order and putting the current bit value in this field and wrapping to start if reached end (circular buffer).
		L	1	Copyright ID start, signals that this frame's Copyright ID bit is the first one by setting 1 and 0 otherwise.
		M	13	Frame length, length of the ADTS frame including headers and CRC check.
		O	11	Buffer fullness, states the bit-reservoir per frame.
		P	2	Number of AAC frames (RDBs (Raw Data Blocks)) in ADTS frame minus 1. For maximum compatibility always use one AAC frame per ADTS frame.
		Q	16	CRC check (as of ISO/IEC 11172-3, subclause 2.4.3.1), if Protection absent is 0.
	*/
		//Get header
		BitReader bitreader(reader);

		Header header = {};

		header.syncWord			 = bitreader.Get(12);
		header.version			 = bitreader.Get(1);
		header.layer			 = bitreader.Get(2);
		header.protectionAbsence	 = bitreader.Get(1);
		header.objectTypeMinus1		 = bitreader.Get(2);
		header.samplingFrequency	 = bitreader.Get(4);
		header.priv			 = bitreader.Get(1);
		header.channelConfiguration	 = bitreader.Get(3);
		header.originality		 = bitreader.Get(1);
		header.home			 = bitreader.Get(1);
		header.copyright		 = bitreader.Get(1);
		header.copyrightStart		 = bitreader.Get(1);
		header.frameLength		 = bitreader.Get(13);
		header.bufferFullness		 = bitreader.Get(11);
		header.numberOfFramesMinus1	 = bitreader.Get(2);

		if (!header.protectionAbsence)
			header.crc		 = bitreader.Get(16);

		//Done reading
		bitreader.Flush();

		//Done
		return header;
	}

}; //namespace mpegts::pes::adts
}; //namespace mpegts::pes
}; //namespace mpegts
