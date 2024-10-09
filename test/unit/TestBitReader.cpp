#include "TestCommon.h"
#include "rtp.h"
#include "bitstream/BitReader.h"
#include "bitstream/BitWriter.h"

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


TEST(TestBitReader, WriteRead)
{

	Logger::EnableDebug(true);

	uint8_t data[4096] = {};

	
	BitWriter w(data,sizeof(data));

	for (size_t j = 1; j <= 32; ++j)
	{
		for (size_t i = 1; i<=32; ++i)
			w.Put(i,i);
		w.Put(j, j);
	}
	w.Flush();


	BufferReader reader(data, sizeof(data));
	BitReader r(reader);
	size_t total = 0;
	for (size_t j = 1; j <= 32; ++j)
	{
		for (size_t i = 1; i <= 32; ++i)
		{
			ASSERT_EQ(r.Get(i), i);
			total += i;
			ASSERT_EQ(reader.Mark(), total / 8 + (total % 8 ? 1 : 0));
		}
		ASSERT_EQ(r.Get(j), j);
		total += j;
		ASSERT_EQ(reader.Mark(), total / 8 + (total % 8 ? 1 : 0));
	}


}