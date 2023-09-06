#ifndef MPEGTS_PSI_H_
#define MPEGTS_PSI_H_

#include <cstdint>
#include <variant>
#include <vector>
#include "BufferReader.h"

namespace mpegts
{

/** parsing of payload units carrying Program Specific Information */
namespace psi
{

/** PSI syntax section, optionally surrounding table data */
struct SyntaxData
{
	// fields preceding table data
	uint16_t tableIdExtension;
	uint8_t _reserved1;
	uint8_t versionNumber;
	bool isCurrent;
	uint8_t sectionNumber;
	uint8_t lastSectionNumber;

	BufferReader data;

	// fields following table data
	uint32_t crc32;

	static SyntaxData Parse(BufferReader& reader);
};

/** PSI table section (checksum not verified if present) */
struct Table
{
	uint8_t tableId;
	bool privateBit;
	uint8_t _reserved1;
	std::variant<BufferReader, SyntaxData> data;

	static Table Parse(BufferReader& reader);
};

/** parse a full PSI payload unit */
std::vector<Table> ParsePayloadUnit(BufferReader& reader);

/** PSI table data for a Program Association Table */
struct ProgramAssociation
{
	static const uint16_t PID = 0x0000;
	static const uint8_t TABLE_ID = 0x00;

	uint16_t programNum;
	uint16_t pmtPid;

	/** parse a single PAT entry */
	static ProgramAssociation Parse(BufferReader& reader);

	/** convenience method: parse a full PSI payload unit containing a PAT (PID = 0) */
	static std::vector<ProgramAssociation> ParsePayloadUnit(BufferReader& reader);
};

/** PSI table data for a Program Map Table */
struct ProgramMap
{
	static const uint8_t TABLE_ID = 0x02;

	/** data for an Elementary Stream entry in a PMT */
	struct ElementaryStream
	{
		uint8_t streamType = 0;
		uint16_t pid = 0;
		uint8_t _reserved1 = 0;
		BufferReader descriptor;

		static ElementaryStream Parse(BufferReader& reader);
	};

	uint16_t pcrPid = 0;
	uint8_t _reserved1 = 0;
	BufferReader programInfo;
	std::vector<ElementaryStream> streams;

	static ProgramMap Parse(BufferReader& reader);
};

}; //namespace mpegts::psi
}; //namespace mpegts

#endif //MPEGTS_PSI_H_
