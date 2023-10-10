
#include "TestCommon.h"

#include "tools.h"

TEST(TestTools, TestAlignMemory)
{
	std::vector<uint8_t> data(256, 0xff);
	
	for (size_t i = 0; i < 110; i++)
	{
		data[i] = 0x1;
	}
	
	ASSERT_EQ(0x1, data[109]);
	ASSERT_EQ(0xff, data[110]);
	ASSERT_EQ(0xff, data[111]);
	ASSERT_EQ(0xff, data[112]);
	
	auto aligned = alignMemory4Bytes(data.data(), 110);
	ASSERT_EQ(aligned, 112);
	
	ASSERT_EQ(0x1, data[109]);
	ASSERT_EQ(0x0, data[110]); // Padded
	ASSERT_EQ(0x0, data[111]); // padded
	ASSERT_EQ(0xff, data[112]);
	
	aligned = alignMemory4Bytes(data.data(), 112);
	ASSERT_EQ(aligned, 112); // No change
	
	ASSERT_EQ(0xff, data[112]);
}
