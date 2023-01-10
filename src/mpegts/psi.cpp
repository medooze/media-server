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

SyntaxData SyntaxData::Parse(BufferReader& reader, const uint8_t*& data, size_t& dataSize)
{
	if (reader.GetLeft()<5+4)
		throw std::runtime_error("Not enough data to read mpegts psi syntax section");

	uint16_t tableIdExtension = reader.Get2();
	uint8_t composite = reader.Get1();
	uint8_t sectionNumber = reader.Get1();
	uint8_t lastSectionNumber = reader.Get1();

	dataSize = reader.GetLeft() - 4;
	data = reader.GetData(dataSize);

	uint32_t crc32 = reader.Get4();

	// save parsed syntax data
	uint8_t _reserved1 = composite >> 6;
	uint8_t versionNumber = (composite >> 1) & 0b11111;
	bool isCurrent = (composite & 1) != 0;
	return { tableIdExtension, _reserved1, versionNumber, isCurrent, sectionNumber, lastSectionNumber, crc32 };
}

Table Table::Parse(BufferReader& reader)
{
	// read table header
	if (reader.GetLeft()<3)
		throw std::runtime_error("Not enough data to read mpegts psi table header");
	BitReader bitreader(reader.GetData(3), 3);

	uint8_t tableId = bitreader.Get(8);
	bool sectionSyntaxIndicator = bitreader.Get(1) != 0;
	bool privateBit = bitreader.Get(1) != 0;
	uint8_t _reserved1 = bitreader.Get(2);
	uint16_t sectionLength = bitreader.Get(12);

	if (sectionLength > 1021)
		throw std::runtime_error("Invalid section length");

	// read table contents (either syntax section or just data)
	if (reader.GetLeft()<sectionLength)
		throw std::runtime_error("Not enough data to read mpegts psi table data");
	BufferReader dataReader (reader.GetData(sectionLength), sectionLength);

	std::optional<SyntaxData> syntax;
	const uint8_t* data;
	size_t dataSize;

	if (sectionSyntaxIndicator) {
		// we have a syntax section surrounding data
		syntax = std::optional(SyntaxData::Parse(dataReader, data, dataSize));
	} else {
		// whole contents are data
		dataSize = dataReader.GetLeft();
		data = dataReader.GetData(dataSize);
		syntax = std::nullopt;
	}

	// return parsed table
	return { tableId, privateBit, _reserved1, syntax, BufferReader(data, dataSize) };
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
	uint16_t programNum = reader.Get2();
	uint16_t pmtPid = GetPID(reader);
	return { programNum, pmtPid };
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
	uint8_t streamType = reader.Get1();
	uint16_t pid = GetPID(reader);
	uint16_t esLength = reader.Get2();

	uint8_t _reserved1 = esLength >> 12;
	esLength = esLength & ~((~0) << 12);

	if (reader.GetLeft()<esLength)
		throw std::runtime_error("Not enough data to read mpegts pmt ES entry descriptor");
	BufferReader descriptor (reader.GetData(esLength), esLength);

	return { streamType, pid, _reserved1, descriptor };
}

// PMT

ProgramMap ProgramMap::Parse(BufferReader& reader)
{
	if (reader.GetLeft()<4)
		throw std::runtime_error("Not enough data to read mpegts pmt header");
	uint16_t pcrPid = GetPID(reader);
	uint16_t piLength = reader.Get2();

	uint8_t _reserved1 = piLength >> 12;
	piLength = piLength & ~((~0) << 12);

	if (reader.GetLeft()<piLength)
		throw std::runtime_error("Not enough data to read mpegts pmt descriptor");
	BufferReader programInfo (reader.GetData(piLength), piLength);

	std::vector<ProgramMap::ES> streams;
	while (reader.GetLeft() > 0)
		streams.push_back(ProgramMap::ES::Parse(reader));

	return { pcrPid, _reserved1, programInfo, streams };
}

}; //namespace mpegts::psi
}; //namespace mpegts
