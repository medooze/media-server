#include "mpegts/psi.h"
#include "log.h"
#include "bitstream.h"
namespace mpegts
{
namespace psi
{

// WARNING: does not check GetLeft() >= 2
static inline uint16_t GetPID(BufferReader& reader)
{
	uint16_t pid = reader.Get2();
	// upper 3 bits are reserved and expected to be ones
	uint8_t reserved = pid >> 13;
	pid = pid & ~((~0) << 13);
	if (reserved != 0b111)
		throw std::runtime_error("invalid PID reserved bits");
	return pid;
}

SyntaxData SyntaxData::Parse(BufferReader& reader, BufferReader& outData)
{
	if (reader.GetLeft()<5+4)
		throw std::runtime_error("Not enough data to read mpegts psi syntax section");

	SyntaxData result = {};
	result.tableIdExtension = reader.Get2();
	uint8_t composite = reader.Get1();
	result.sectionNumber = reader.Get1();
	result.lastSectionNumber = reader.Get1();

	size_t dataSize = reader.GetLeft() - 4;
	outData = BufferReader(reader.GetData(dataSize), dataSize);

	result.crc32 = reader.Get4();

	// save parsed syntax data
	result._reserved1 = composite >> 6;
	result.versionNumber = (composite >> 1) & 0b11111;
	result.isCurrent = (composite & 1) != 0;
	return result;
}

Table Table::Parse(BufferReader& reader)
{
	Table result;

	// read table header
	if (reader.GetLeft()<3)
		throw std::runtime_error("Not enough data to read mpegts psi table header");
	BitReader bitreader(reader.GetData(3), 3);

	result.tableId = bitreader.Get(8);
	bool sectionSyntaxIndicator = bitreader.Get(1) != 0;
	result.privateBit = bitreader.Get(1) != 0;
	result._reserved1 = bitreader.Get(2);
	uint16_t sectionLength = bitreader.Get(12);

	if (sectionLength > 1021)
		throw std::runtime_error("Invalid section length");

	// read table contents (either syntax section or just data)
	if (reader.GetLeft()<sectionLength)
		throw std::runtime_error("Not enough data to read mpegts psi table data");
	BufferReader dataReader (reader.GetData(sectionLength), sectionLength);

	if (sectionSyntaxIndicator) {
		// we have a syntax section surrounding data
		result.syntax = std::optional(SyntaxData::Parse(dataReader, result.data));
	} else {
		// whole contents are data
		size_t dataSize = dataReader.GetLeft();
		result.data = BufferReader(dataReader.GetData(dataSize), dataSize);
	}

	// return parsed table
	return result;
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
	while (reader.GetLeft() > 0 && reader.Peek1() != 0xFF) {
		// FIXME: it would probably be nice to catch parsing errors and return partial result
		tables.push_back(Table::Parse(reader));
	}
	return tables;
}

// PAT

ProgramAssociation ProgramAssociation::Parse(BufferReader& reader)
{
	if (reader.GetLeft()<4)
		throw std::runtime_error("Not enough data to read mpegts pat entry");
	ProgramAssociation result;
	result.programNum = reader.Get2();
	result.pmtPid = GetPID(reader);
	return result;
}

std::vector<ProgramAssociation> ProgramAssociation::ParsePayloadUnit(BufferReader& reader)
{
	auto tables = mpegts::psi::ParsePayloadUnit(reader);

	// FIXME: this is probably too strict
	// FIXME: maybe check CRCs

	if (tables.size() != 1)
		throw std::runtime_error("PAT did not contain exactly one table");
	if (!(
		tables[0].tableId == ProgramAssociation::TABLE_ID &&
		tables[0].privateBit == false &&
		tables[0].syntax &&
		tables[0].syntax->isCurrent
	))
		throw std::runtime_error("malformed PAT table section");

	BufferReader dataReader = tables[0].data;
	std::vector<ProgramAssociation> entries;
	while (dataReader.GetLeft() > 0)
		entries.push_back(ProgramAssociation::Parse(dataReader));
	return entries;
}

ProgramMap::ES ProgramMap::ES::Parse(BufferReader& reader)
{
	if (reader.GetLeft()<5)
		throw std::runtime_error("Not enough data to read mpegts pmt ES entry header");
	ProgramMap::ES result;
	result.streamType = reader.Get1();
	result.pid = GetPID(reader);
	uint16_t esLength = reader.Get2();

	result._reserved1 = esLength >> 12;
	esLength = esLength & ~((~0) << 12);

	if (reader.GetLeft()<esLength)
		throw std::runtime_error("Not enough data to read mpegts pmt ES entry descriptor");
	result.descriptor = BufferReader(reader.GetData(esLength), esLength);

	return result;
}

// PMT

ProgramMap ProgramMap::Parse(BufferReader& reader)
{
	if (reader.GetLeft()<4)
		throw std::runtime_error("Not enough data to read mpegts pmt header");

	ProgramMap result;
	result.pcrPid = GetPID(reader);
	uint16_t piLength = reader.Get2();

	result._reserved1 = piLength >> 12;
	piLength = piLength & ~((~0) << 12);

	if (reader.GetLeft()<piLength)
		throw std::runtime_error("Not enough data to read mpegts pmt descriptor");
	result.programInfo = BufferReader(reader.GetData(piLength), piLength);

	while (reader.GetLeft() > 0)
		result.streams.push_back(ProgramMap::ES::Parse(reader));

	return result;
}

}; //namespace mpegts::psi
}; //namespace mpegts
