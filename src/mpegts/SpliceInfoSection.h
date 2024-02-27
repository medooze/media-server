#ifndef SPLICEINFOSECTION_H
#define SPLICEINFOSECTION_H

#include <vector>
#include <sstream>

class SpliceInfoSection
{
public:
	size_t Parse(const uint8_t *data, size_t len);
	
	void Dump() const;
	
private:
	uint8_t table_id = 0;
	uint8_t section_syntax_indicator = 0;
	uint8_t private_indicator = 0;
	uint8_t sap_type = 0;
	uint16_t section_length = 0;
	
	friend class TestSpliceInfoSection;
};


#endif
