#ifndef MPEGTS_PSI_H_
#define MPEGTS_PSI_H_

#include <cstdint>
#include <variant>
#include <vector>
#include "BufferReader.h"
#include "BufferWritter.h"

namespace mpegts
{

/** parsing of payload units carrying Program Specific Information */
namespace psi
{

/** PSI table data for a Program Association Table */
struct ProgramAssociation
{
	static constexpr uint16_t PID = 0x0000;
	static constexpr uint8_t TABLE_ID = 0x00;

	uint16_t programNum = 0;
	uint16_t pmtPid = 0;

	void Serialize(BufferWritter& writer) const;
	size_t GetSize() const;
	
	/** parse a single PAT entry */
	static ProgramAssociation Parse(BufferReader& reader);

	/** convenience method: parse a full PSI payload unit containing a PAT (PID = 0) */
	static std::vector<ProgramAssociation> ParsePayloadUnit(BufferReader& reader);
};

/** PSI table data for a Program Map Table */
struct ProgramMap
{
	static constexpr uint8_t TABLE_ID = 0x02;

	/** data for an Elementary Stream entry in a PMT */
	struct ElementaryStream
	{
		/**
		 * This is part of stream type values.
		 * 
		 * See ISO/IEC 13818-1 section 2.4.4.10 for complete list.
		 */
		enum StreamType
		{
			MPEG1Audio = 0x03,
			MPEG2Audio = 0x04,
			ADTSAAC = 0x0F,
			AVC = 0x1B,
			HEVC = 0x24
		};

		uint8_t streamType = 0;
		uint16_t pid = 0;
		
		BufferReader descriptor;
		
		void Serialize(BufferWritter& writer) const;
		size_t GetSize() const;

		static ElementaryStream Parse(BufferReader& reader);
	};

	uint16_t pcrPid = 0;
	
	std::vector<ElementaryStream> streams;

	void Serialize(BufferWritter& writer) const;
	size_t GetSize() const;
	
	static ProgramMap Parse(BufferReader& reader);
};


/** PSI syntax section, optionally surrounding table data */
struct SyntaxData
{
	// fields preceding table data
	uint16_t tableIdExtension = 0;
	uint8_t _reserved1 = 0x3;
	uint8_t versionNumber = 0;
	bool isCurrent = false;
	uint8_t sectionNumber = 0;
	uint8_t lastSectionNumber = 0;

	std::variant<BufferReader, ProgramMap, ProgramAssociation> data;

	void Serialize(BufferWritter& writer) const;
	size_t GetSize() const;
	
	static SyntaxData Parse(BufferReader& reader);
};

/** PSI table section (checksum not verified if present) */
struct Table
{
	uint8_t tableId = 0;
	bool sectionSyntaxIndicator = false;
	bool privateBit = false;
	uint8_t _reserved1 = 0x3;
	
	std::variant<BufferReader, SyntaxData> data;
	
	void Serialize(BufferWritter& writer) const;
	size_t GetSize() const;
	
	static Table Parse(BufferReader& reader);
};


/** parse a full PSI payload unit */
std::vector<Table> ParsePayloadUnit(BufferReader& reader);


}; //namespace mpegts::psi
}; //namespace mpegts

#endif //MPEGTS_PSI_H_
