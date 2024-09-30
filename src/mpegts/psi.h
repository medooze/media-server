#ifndef MPEGTS_PSI_H_
#define MPEGTS_PSI_H_

#include <cstdint>
#include <variant>
#include <vector>
#include "BufferReader.h"
#include "BufferWritter.h"
#include "mpegts.h"

namespace mpegts
{

/** parsing of payload units carrying Program Specific Information */
namespace psi
{


/** PSI table data for a Program Association Table */
struct ProgramAssociation : public Encodable
{
	static constexpr uint16_t PID = 0x0000;
	static constexpr uint8_t TABLE_ID = 0x00;

	uint16_t programNum = 0;
	uint16_t pmtPid = 0;

	void Encode(BufferWritter& writer) const override;
	size_t Size() const override;
	
	/** parse a single PAT entry */
	static ProgramAssociation Parse(BufferReader& reader);
};

/** PSI table data for a Program Map Table */
struct ProgramMap : public Encodable
{
	static constexpr uint8_t TABLE_ID = 0x02;

	/** data for an Elementary Stream entry in a PMT */
	struct ElementaryStream : public Encodable
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
		
		void Encode(BufferWritter& writer) const override;
		size_t Size() const override;

		static ElementaryStream Parse(BufferReader& reader);
	};

	uint16_t pcrPid = 0;
	
	std::vector<ElementaryStream> streams;

	void Encode(BufferWritter& writer) const override;
	size_t Size() const override;
	
	static ProgramMap Parse(BufferReader& reader);
};


/** PSI syntax section, optionally surrounding table data */
struct SyntaxData : public Encodable
{
	// fields preceding table data
	uint16_t tableIdExtension = 0;
	uint8_t _reserved1 = 0x3;
	uint8_t versionNumber = 0;
	bool isCurrent = false;
	uint8_t sectionNumber = 0;
	uint8_t lastSectionNumber = 0;

	std::variant<BufferReader, ProgramMap, ProgramAssociation> data;

	void Encode(BufferWritter& writer) const override;
	size_t Size() const override;
	
	static SyntaxData Parse(BufferReader& reader);
};

/** PSI table section (checksum not verified if present) */
struct Table : public Encodable
{
	uint8_t tableId = 0;
	bool sectionSyntaxIndicator = false;
	bool privateBit = false;
	uint8_t _reserved1 = 0x3;
	
	std::variant<BufferReader, SyntaxData> data;
	
	void Encode(BufferWritter& writer) const override;
	size_t Size() const override;
	
	static Table Parse(BufferReader& reader);
};

}; //namespace mpegts::psi
}; //namespace mpegts

#endif //MPEGTS_PSI_H_
