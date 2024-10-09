#include "mpegts/psi.h"
#include "log.h"
#include "bitstream/BitReader.h"
#include "bitstream/BitWriter.h"

#include "mpegtscrc32.h"

namespace mpegts
{
namespace psi
{

void SyntaxData::Serialize(BufferWritter& writer) const
{
	if (writer.GetLeft() < GetSize())
		throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
	
	writer.Set2(tableIdExtension);
	
	BitWriter bitWriter(writer, 3);
	bitWriter.Put(2, _reserved1);
	bitWriter.Put(5, versionNumber);
	bitWriter.Put(1, isCurrent);
	bitWriter.Put(8, sectionNumber);
	bitWriter.Put(8, lastSectionNumber);
		
	std::visit([&writer](auto&& arg){
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, ProgramAssociation> || std::is_same_v<T, ProgramMap>)
		{
			arg.Serialize(writer);
		}
		else if constexpr (std::is_same_v<T, BufferReader>)
		{
			auto reader = arg;
			size_t num = reader.GetLeft();
			memcpy(writer.Consume(num), reader.GetData(num), num); 
		}
	}, data);
}

size_t SyntaxData::GetSize() const
{
	size_t totalSize = 5;
		
	std::visit([&totalSize](auto&& arg){
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, ProgramAssociation> || std::is_same_v<T, ProgramMap>)
		{
			totalSize += arg.GetSize();
		}
		else if constexpr (std::is_same_v<T, BufferReader>)
		{
			totalSize += arg.GetLeft();
		}
	}, data);
		
	return totalSize;
}


SyntaxData SyntaxData::Parse(BufferReader& reader)
{
	SyntaxData syntaxData = {};

	/*
	 * Table ID extension		16	Informational only identifier. The PAT uses this for the transport stream identifier and the PMT uses this for the Program num
	 * Reserved bits		2	Set to 0x03 (all bits on)
	 * Version number		5	Syntax version number. Incremented when data is changed and wrapped around on overflow for values greater than 32.
	 * Current/next indicator	1	Indicates if data is current in effect or is for future use. If the bit is flagged on, then the data is to be used at the present moment.
	 * Section number		8	This is an index indicating which table this is in a related sequence of tables. The first table starts from 0.
	 * Last section number		8	This indicates which table is the last table in the sequence of tables.
	 * Table data			N*8	Data as defined by the Table Identifier.
	 * CRC32			32	A checksum of the entire table excluding the pointer field, pointer filler bytes and the trailing CRC32.
	 */

	if (reader.GetLeft()<5+4)
		throw std::runtime_error("Not enough data to read mpegts psi syntax section");

	BitReader bitreader(reader);

	syntaxData.tableIdExtension	= bitreader.Get(16);
	syntaxData._reserved1		= bitreader.Get(2);
	syntaxData.versionNumber	= bitreader.Get(5);
	syntaxData.isCurrent		= bitreader.Get(1) != 0;
	syntaxData.sectionNumber	= bitreader.Get(8);
	syntaxData.lastSectionNumber	= bitreader.Get(8);

	bitreader.Flush();

	size_t dataSize = reader.GetLeft() - 4;
	syntaxData.data			= BufferReader(reader.GetData(dataSize), dataSize);
	
	return syntaxData;
}

void Table::Serialize(BufferWritter& writer) const
{
	if (writer.GetLeft() < GetSize())
		throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
	
	auto mark = writer.Mark();
	
	writer.Set1(tableId);
	
	BitWriter bitwriter(writer, 2);
	
	bitwriter.Put(1, sectionSyntaxIndicator);
	bitwriter.Put(1, privateBit);
	bitwriter.Put(2, _reserved1);
	bitwriter.Put(12, GetSize() - 3);
		
	std::visit([&writer](auto&& arg){
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, SyntaxData>)
		{
			arg.Serialize(writer);
		}
		else if constexpr (std::is_same_v<T, BufferReader>)
		{
			auto reader = arg;
			size_t num = reader.GetLeft();
			memcpy(writer.Consume(num), reader.GetData(num), num); 
		}
	}, data);
		
	uint32_t crc = Crc32(writer.GetData() + mark, writer.GetLength() - mark);
	writer.Set4(crc);
}

size_t Table::GetSize() const
{
	size_t totalSize = 3 + 4;  // header + crc
	
	std::visit([&totalSize](auto&& arg){
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, SyntaxData>)
		{
			totalSize += arg.GetSize();
		}
		else if constexpr (std::is_same_v<T, BufferReader>)
		{
			totalSize += arg.GetLeft();
		}
	}, data);
	
	return totalSize;
}

Table Table::Parse(BufferReader& reader)
{
	Table table = {};

	/*
	 * Table ID			8	Table Identifier, that defines the structure of the syntax section and other contained data.
	 *					As an exception, if this is the byte that immediately follow previous table section and is set to 0xFF, then it indicates that the repeat of table section end here and the rest of TS packet payload shall be stuffed with 0xFF.
	 *					Consequently, the value 0xFF shall not be used for the Table Identifier.
	 * Section syntax indicator	1	A flag that indicates if the syntax section follows the section length. The PAT, PMT, and CAT all set this to 1.
	 * Private bit			1	The PAT, PMT, and CAT all set this to 0. Other tables set this to 1.
	 * Reserved bits		2	Set to 0x03 (all bits on)
	 * Section length unused bits	2	Set to 0 (all bits off)
	 * Section length		10	The number of bytes that follow for the syntax section (with CRC value) and/or table data. These bytes must not exceed a value of 1021.
	 * Syntax section/Table data	N*8	When the section length is non-zero, this is the section length number of syntax and data bytes.
	 */

	// read table header
	if (reader.GetLeft()<3)
		throw std::runtime_error("Not enough data to read mpegts psi table header");

	BitReader bitreader(reader);

	table.tableId			= bitreader.Get(8);
	table.sectionSyntaxIndicator	= bitreader.Get(1) != 0;
	table.privateBit		= bitreader.Get(1) != 0;
	table._reserved1		= bitreader.Get(2);
	bitreader.Skip(2);		//length unused bits
	auto sectionLength		= bitreader.Get(10);

	bitreader.Flush();

	// read table contents (either syntax section or just data)
	if (reader.GetLeft()<sectionLength)
		throw std::runtime_error("Not enough data to read mpegts psi table data");

	BufferReader dataReader (reader.GetData(sectionLength), sectionLength);

	if (table.sectionSyntaxIndicator) 
	{
		// we have a syntax section surrounding data
		table.data = SyntaxData::Parse(dataReader);
		
		auto crc32 = reader.Get4();
		(void)crc32;

		//TODO: check crc?
	} 
	else 
	{
		// whole contents are data
		table.data = dataReader;
	}

	// return parsed table
	return table;
}

std::vector<Table> ParsePayloadUnit(BufferReader& reader)
{
	// read pointer
	if (reader.GetLeft()<1)
		throw std::runtime_error("Not enough data to read mpegts psi pointer");
	uint8_t pointerField = reader.Get1();
	if (reader.GetLeft()<pointerField)
		throw std::runtime_error("Not enough data to read mpegts psi pointer filler bytes");
	reader.GetData(pointerField);

	// read tables until EOF or 0xFF byte (which is followed by stuffing)
	std::vector<Table> tables;
	while (reader.GetLeft() > 0 && reader.Peek1() != 0xFF)
		// FIXME: it would probably be nice to catch parsing errors and return partial result
		tables.push_back(Table::Parse(reader));

	return tables;
}

// PAT

void ProgramAssociation::Serialize(BufferWritter& writer) const
{
	if (writer.GetLeft() < GetSize())
		throw std::runtime_error("Not enough data in function " + std::string(__FUNCTION__));
		
	writer.Set2(programNum);
	
	BitWriter bitwriter(writer, 2);
	
	bitwriter.Put(3, 0x7); //reserved
	bitwriter.Put(13, pmtPid);
}

size_t ProgramAssociation::GetSize() const
{ 
	return 4;
}

ProgramAssociation ProgramAssociation::Parse(BufferReader& reader)
{
	ProgramAssociation programAssociation = {};

	/*
	 * Program num		16	Relates to the Table ID extension in the associated PMT. A value of 0 is reserved for a NIT packet identifier.
	 * Reserved bits	3	Set to 0x07 (all bits on)
	 * Program map PID	13	The packet identifier that contains the associated PMT
	 */

	if (reader.GetLeft()<4)
		throw std::runtime_error("Not enough data to read mpegts pat entry");
	
	BitReader bitreader(reader);

	programAssociation.programNum	= bitreader.Get(16);
	bitreader.Skip(3);		//Reserved
	programAssociation.pmtPid	= bitreader.Get(13);

	bitreader.Flush();

	return programAssociation;
}

std::vector<ProgramAssociation> ProgramAssociation::ParsePayloadUnit(BufferReader& reader)
{
	auto tables = mpegts::psi::ParsePayloadUnit(reader);

	// FIXME: this is probably too strict
	// FIXME: maybe check CRCs

	if (tables.size() != 1)
		throw std::runtime_error("PAT did not contain exactly one table");
	auto syntax = std::get_if<SyntaxData>(&tables[0].data);
	if (!(
		tables[0].tableId == ProgramAssociation::TABLE_ID &&
		tables[0].privateBit == false &&
		syntax &&
		syntax->isCurrent
	))
		throw std::runtime_error("malformed PAT table section");

	BufferReader* dataReader = std::get_if<BufferReader>(&syntax->data);
	if (dataReader == nullptr)
		throw std::runtime_error("syntax data is not available");
		
	std::vector<ProgramAssociation> entries;
	while (dataReader->GetLeft() > 0)
		entries.push_back(ProgramAssociation::Parse(*dataReader));

	return entries;
}


void ProgramMap::ElementaryStream::Serialize(BufferWritter& writer) const
{
	BitWriter bitwriter(writer, 5);
	
	bitwriter.Put(8, streamType);
	bitwriter.Put(3, 0x7);	// reserved
	bitwriter.Put(13, pid);
	bitwriter.Put(4, 0xf); // reserved
	bitwriter.Put(12, descriptor.GetLeft());
	
	if (descriptor.GetLeft() > 0)
	{
		auto reader = descriptor;
		auto buffer = reader.GetBuffer(reader.GetLeft());
	
		writer.Set(buffer);
	}
}

size_t ProgramMap::ElementaryStream::GetSize() const
{
	size_t totalSize = 5;
		
	return totalSize + descriptor.GetLeft();
}

ProgramMap::ElementaryStream ProgramMap::ElementaryStream::Parse(BufferReader& reader)
{
	ProgramMap::ElementaryStream elementaryStream = {};

	/*
	 * stream type				8	This defines the structure of the data contained within the elementary packet identifier.
	 * Reserved bits			3	Set to 0x07 (all bits on)
	 * Elementary PID			13	The packet identifier that contains the stream type data.
	 * Reserved bits			4	Set to 0x0F (all bits on)
	 * ES Info length unused bits		2	Set to 0 (all bits off)
	 * ES Info length length		10	The number of bytes that follow for the elementary stream descriptors.
	 * Elementary stream descriptors	N*8	When the ES info length is non-zero, this is the ES info length number of elementary stream descriptor bytes.
	 */

	if (reader.GetLeft()<5)
		throw std::runtime_error("Not enough data to read mpegts pmt ES entry header");

	BitReader bitreader(reader);

	elementaryStream.streamType	= bitreader.Get(8);
	bitreader.Skip(3);		//Reserved
	elementaryStream.pid		= bitreader.Get(13);
	bitreader.Skip(4);		//Reserved
	bitreader.Skip(2);		//Unused
	uint16_t esLength		= bitreader.Get(10);

	bitreader.Flush();

	if (reader.GetLeft()<esLength)
		throw std::runtime_error("Not enough data to read mpegts pmt ES entry descriptor");
	elementaryStream.descriptor	= BufferReader(reader.GetData(esLength), esLength);

	return elementaryStream;
}

// PMT

void ProgramMap::Serialize(BufferWritter& writer) const
{
	BitWriter bitwriter(writer, 4);
	
	bitwriter.Put(3, 0x7);		// Reserved
	bitwriter.Put(13, pcrPid);	// PCR_PID, no PCR
	bitwriter.Put(4, 0xf);		// reserved
	bitwriter.Put(12, 0);		// program info length
	
	for (auto& es : streams) 
	{
		es.Serialize(writer);
	}
}

size_t ProgramMap::GetSize() const
{ 
	size_t totalSize = 4;
	
	for (auto& es : streams) 
	{
		totalSize  += es.GetSize();
	}
	
	return totalSize;
}

ProgramMap ProgramMap::Parse(BufferReader& reader)
{
	ProgramMap programMap = {};

	/*
	 * Reserved bits			3	Set to 0x07 (all bits on)
	 * PCR PID				13	The packet identifier that contains the program clock reference used to improve the random access accuracy of the stream's timing that is derived from the program timestamp.
	 *						If this is unused. then it is set to 0x1FFF (all bits on).
	 * Reserved bits			4	Set to 0x0F (all bits on)
	 * Program info length unused bits	2	Set to 0 (all bits off)
	 * Program info length			10	The number of bytes that follow for the program descriptors.
	 * Program descriptors			N*8	When the program info length is non-zero, this is the program info length number of program descriptor bytes.
	 * Elementary stream info data		N*8	The streams used in this program map.
	 */

	if (reader.GetLeft()<4)
		throw std::runtime_error("Not enough data to read mpegts pmt header");

	BitReader bitreader(reader);

	bitreader.Skip(3);	//Reserved
	programMap.pcrPid	= bitreader.Get(13);
	bitreader.Skip(4);	//Reserved
	bitreader.Skip(2);	//Unused
	auto piLength	= bitreader.Get(10);

	bitreader.Flush();

	if (reader.GetLeft()<piLength)
		throw std::runtime_error("Not enough data to read mpegts pmt descriptor");

	while (reader.GetLeft() > 0)
		programMap.streams.push_back(ProgramMap::ElementaryStream::Parse(reader));

	return programMap;
}

}; //namespace mpegts::psi
}; //namespace mpegts
