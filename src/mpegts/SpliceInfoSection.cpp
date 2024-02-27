#include "SpliceInfoSection.h"
#include "log.h"
#include "bitstream.h"

size_t SpliceInfoSection::Parse(const uint8_t *data, size_t len)
{
	constexpr size_t SizeUntilSectionLengthField = 3;
	if (len < SizeUntilSectionLengthField) return 0;
	
	BitReader reader(data, len);
	
	table_id = reader.Get(8);
	constexpr uint8_t SpliceInfoSectionTableId = 0xFC;
	if (table_id != SpliceInfoSectionTableId)
	{
		// Not a splice info section
		return 0;
	}
	
	section_syntax_indicator = reader.Get(1);
	private_indicator = reader.Get(1);
	sap_type = reader.Get(2);
	section_length = reader.Get(12);
	
	if (section_length > (len - SizeUntilSectionLengthField))
	{
		// The data is not complete
		return 0;
	}

	return section_length + SizeUntilSectionLengthField;
}

void SpliceInfoSection::Dump() const
{
	Debug("table_id: %d\n", table_id);
	Debug("section_syntax_indicator: %d\n", section_syntax_indicator);
	Debug("private_indicator: %d\n", private_indicator);
	Debug("sap_type: %d\n", sap_type);
	Debug("section_length: %d\n", section_length);
}
