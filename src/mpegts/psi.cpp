#include "mpegts/psi.h"
#include "log.h"
#include "bitstream.h"
namespace mpegts
{
namespace psi
{

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

	BitReader bitreader(reader, 5);

	syntaxData.tableIdExtension	= bitreader.Get(16);
	syntaxData._reserved1		= bitreader.Get(2);
	syntaxData.versionNumber	= bitreader.Get(5);
	syntaxData.isCurrent		= bitreader.Get(1) != 0;
	syntaxData.sectionNumber	= bitreader.Get(8);
	syntaxData.lastSectionNumber	= bitreader.Get(8);

	size_t dataSize = reader.GetLeft() - 4;
	syntaxData.data			= BufferReader(reader.GetData(dataSize), dataSize);

	syntaxData.crc32		= reader.Get4();

	//TODO: check crc?
	
	return syntaxData;
}

Table Table::Parse(BufferReader& reader)
{
	Table table;

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

	BitReader bitreader(reader, 3);

	table.tableId			= bitreader.Get(8);
	bool sectionSyntaxIndicator	= bitreader.Get(1) != 0;
	table.privateBit		= bitreader.Get(1) != 0;
	table._reserved1		= bitreader.Get(2);
	bitreader.Skip(2);		//length unused bits
	uint16_t sectionLength		= bitreader.Get(10);

	// read table contents (either syntax section or just data)
	if (reader.GetLeft()<sectionLength)
		throw std::runtime_error("Not enough data to read mpegts psi table data");

	BufferReader dataReader (reader.GetData(sectionLength), sectionLength);

	if (sectionSyntaxIndicator) 
	{
		// we have a syntax section surrounding data
		table.data = SyntaxData::Parse(dataReader);
	} else {
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

ProgramAssociation ProgramAssociation::Parse(BufferReader& reader)
{
	ProgramAssociation programAssociation;

	/*
	 * Program num		16	Relates to the Table ID extension in the associated PMT. A value of 0 is reserved for a NIT packet identifier.
	 * Reserved bits	3	Set to 0x07 (all bits on)
	 * Program map PID	13	The packet identifier that contains the associated PMT
	 */

	if (reader.GetLeft()<4)
		throw std::runtime_error("Not enough data to read mpegts pat entry");
	
	BitReader bitreader(reader, 4);

	programAssociation.programNum	= bitreader.Get(16);
	bitreader.Skip(3);		//Reserved
	programAssociation.pmtPid	= bitreader.Get(13);

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

	BufferReader dataReader = syntax->data;
	std::vector<ProgramAssociation> entries;
	while (dataReader.GetLeft() > 0)
		entries.push_back(ProgramAssociation::Parse(dataReader));

	return entries;
}

ProgramMap::ElementaryStream ProgramMap::ElementaryStream::Parse(BufferReader& reader)
{
	ProgramMap::ElementaryStream elementaryStream;

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

	BitReader bitreader(reader, 5);

	elementaryStream.streamType	= bitreader.Get(8);
	bitreader.Skip(3);		//Reserved
	elementaryStream.pid		= bitreader.Get(13);
	bitreader.Skip(4);		//Reserved
	bitreader.Skip(2);		//Unused
	uint16_t esLength		= bitreader.Get(10);

	if (reader.GetLeft()<esLength)
		throw std::runtime_error("Not enough data to read mpegts pmt ES entry descriptor");
	elementaryStream.descriptor	= BufferReader(reader.GetData(esLength), esLength);

	return elementaryStream;
}

// PMT

ProgramMap ProgramMap::Parse(BufferReader& reader)
{
	ProgramMap programMap;

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

	BitReader bitreader(reader, 4);

	bitreader.Skip(3);	//Reserved
	programMap.pcrPid	= bitreader.Get(13);
	bitreader.Skip(4);	//Reserved
	bitreader.Skip(2);	//Unused
	uint16_t piLength	= bitreader.Get(10);

	if (reader.GetLeft()<piLength)
		throw std::runtime_error("Not enough data to read mpegts pmt descriptor");

	programMap.programInfo = BufferReader(reader.GetData(piLength), piLength);

	while (reader.GetLeft() > 0)
		programMap.streams.push_back(ProgramMap::ElementaryStream::Parse(reader));

	return programMap;
}

}; //namespace mpegts::psi
}; //namespace mpegts
