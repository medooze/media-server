#include "TestCommon.h"

#include "mpegts/mpegtscrc32.h"

TEST(TestCrc32, MPEGTSCrc)
{
	const uint8_t data[] = {0x01, 0x03, 0x05, 0x08, 0xe0, 0x60};
	ASSERT_EQ(0x1BD4EF72, mpegts::Crc32(data, sizeof(data)));
}
