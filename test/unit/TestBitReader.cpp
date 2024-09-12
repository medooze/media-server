#include "TestCommon.h"
#include "rtp.h"
#include "bitstream/BitReader.h"

TEST(TestBitReader, BufferReaderPos)
{

	Logger::EnableDebug(true);

	uint8_t data[256] = {};

	for (size_t num = 1; num <= 32; num++ )
	{
		size_t total = 0;
		BufferReader reader(data, sizeof(data));
		BitReader r(reader);

		ASSERT_EQ(reader.Mark(), 0);

		while (total + num < sizeof(data) * 8 )
		{
			r.Get(num);
			total += num;
			ASSERT_EQ(reader.Mark(), total/8 + (total % 8 ? 1 : 0) );
			
		}
	}

	for (size_t num = 1; num <= 32; num++)
	{
		size_t total = 0;
		BufferReader reader(data, sizeof(data));
		BitReader r(reader);

		ASSERT_EQ(reader.Mark(), 0);

		while (total + num < sizeof(data) * 8)
		{
			r.Skip(num);
			total += num;
			ASSERT_EQ(reader.Mark(), total / 8 + (total % 8 ? 1 : 0));

		}
	}
}

