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

/** PSI syntax section, optionally surrounding table data */
struct SyntaxData : public Encodable
{
	// fields preceding table data
	uint16_t tableIdExtension;
	uint8_t _reserved1;
	uint8_t versionNumber;
	bool isCurrent;
	uint8_t sectionNumber;
	uint8_t lastSectionNumber;

	std::variant<BufferReader, std::unique_ptr<Encodable>> data;

	// fields following table data
	uint32_t crc32;

	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	static std::unique_ptr<SyntaxData> Parse(BufferReader& reader);
};

/** PSI table section (checksum not verified if present) */
struct Table : public Encodable
{
	uint8_t tableId = 0;
	bool sectionSyntaxIndicator = false;
	bool privateBit = false;
	uint8_t _reserved1 = 0x3;
	uint16_t sectionLength = 0;
	
	std::variant<BufferReader, std::unique_ptr<Encodable>> data;
	
	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	static std::unique_ptr<Table> Parse(BufferReader& reader);
};

/** PSI table data for a Program Association Table */
struct ProgramAssociation : public Encodable
{
	static const uint16_t PID = 0x0000;
	static const uint8_t TABLE_ID = 0x00;

	uint16_t programNum = 0;
	uint16_t pmtPid = 0;

	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	/** parse a single PAT entry */
	static ProgramAssociation Parse(BufferReader& reader);
};

/** PSI table data for a Program Map Table */
struct ProgramMap : public Encodable
{
	static const uint8_t TABLE_ID = 0x02;

	/** data for an Elementary Stream entry in a PMT */
	struct ElementaryStream : public Encodable
	{
		uint8_t streamType = 0;
		uint16_t pid = 0;
		
		BufferReader descriptor;
		
		void Encode(BufferWritter& writer) override;
		size_t Size() const override;

		static ElementaryStream Parse(BufferReader& reader);
	};

	uint16_t pcrPid = 0;
	uint16_t piLength = 0;
	
	std::vector<ElementaryStream> streams;

	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	static ProgramMap Parse(BufferReader& reader);
};

template <typename T>
T DowncastEncodable(const std::variant<BufferReader, std::unique_ptr<Encodable>>& data)
{
	auto encodable = std::get_if<std::unique_ptr<Encodable>>(&data);
	
	if (!encodable)
		throw std::runtime_error("failed to downcast Encodable object");

	return static_cast<T>(encodable->get());
}

}; //namespace mpegts::psi
}; //namespace mpegts

#endif //MPEGTS_PSI_H_
