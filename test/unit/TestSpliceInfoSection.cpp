#include "TestCommon.h"
#include "mpegts/SpliceInfoSection.h"

#include <vector>

std::vector<uint8_t> HexToBin(char* hex, size_t len)
{
	size_t outSize = len >> 1;
	std::vector<uint8_t> out(outSize);
	
	char tmp[3];
	for (size_t i = 0; i < outSize; i++)
	{
		memcpy(tmp, hex + 2*i, 2);
		tmp[2] = 0;
		out[i] = static_cast<unsigned char>(strtol(tmp, nullptr, 16));
	}
	
	return out;
}

class TestSpliceInfoSection : public testing::Test
{
public:
	void TestParsing();
};

void TestSpliceInfoSection::TestParsing()
{
	char hex[] = "fc302000000002cadb00fff00f05000000007ffffe00a4e9f5007001010000dd9f4348";
	auto bin = HexToBin(hex, sizeof(hex));
	
	SpliceInfoSection sis;
	
	ASSERT_EQ(35, sis.Parse(bin.data(), bin.size()));
	ASSERT_EQ(0xfc, sis.table_id);
	ASSERT_EQ(0, sis.section_syntax_indicator);
	ASSERT_EQ(3, sis.sap_type);
	ASSERT_EQ(0, sis.private_indicator);
	ASSERT_EQ(32, sis.section_length);
	
	ASSERT_EQ(0, sis.Parse(bin.data(), bin.size() - 1));
}


TEST_F(TestSpliceInfoSection, TestPasring)
{
	ASSERT_NO_FATAL_FAILURE(TestParsing());
}