#include "TestCommon.h"

#include "CircularBuffer.h"


TEST(TestCircularBuffer, Secuential)
{
	CircularBuffer<uint16_t, uint8_t, 10> buffer;

	ASSERT_FALSE(buffer.IsPresent(0));

	for (uint16_t i = 0; i < 399; ++i)
	{
		ASSERT_TRUE(buffer.Set(i, i));
		//Log("%i %i %i %i %d\n", i, buffer.Get(i).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
		ASSERT_TRUE(buffer.IsPresent(i));
		ASSERT_EQ(buffer.Get(i).value(), i);
		ASSERT_EQ(buffer.GetLastSeq(), i % 256);
	}

	ASSERT_FALSE(buffer.Set(buffer.GetLastSeq() - 11, 0));
}

TEST(TestCircularBuffer, Evens)
{
	CircularBuffer<uint16_t, uint8_t, 10> buffer;

	ASSERT_FALSE(buffer.IsPresent(0));

	for (uint16_t i = 0; i < 399; ++i)
	{
		ASSERT_TRUE(buffer.Set(i * 2, i));
		//Log("%i %i %i %i %d\n", i, buffer.Get(i * 2).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
		ASSERT_TRUE(buffer.IsPresent(i * 2));
		ASSERT_EQ(buffer.Get(i * 2).value(), i);
		ASSERT_EQ(buffer.GetLastSeq(), (i * 2) % 256);
		if (i)
			ASSERT_FALSE(buffer.IsPresent(i * 2 - 1));
	}

	ASSERT_FALSE(buffer.Set(buffer.GetLastSeq() - 11, 0));
}


TEST(TestCircularBuffer, BigJumps)
{
	CircularBuffer<uint16_t, uint8_t, 10> buffer;

	ASSERT_FALSE(buffer.IsPresent(0));
	for (uint16_t i = 0; i < 399; ++i)
	{
		ASSERT_TRUE(buffer.Set(i * 20, i));
		//Log("%i %i %i %i %d\n", i, buffer.Get(i * 20).value(), buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
		ASSERT_TRUE(buffer.IsPresent(i * 20));
		ASSERT_EQ(buffer.Get(i * 20).value(), i);
		ASSERT_EQ(buffer.GetLastSeq(), (i * 20) % 256);
		if (i)
			ASSERT_FALSE(buffer.IsPresent(i * 20 - 1));
	}

	ASSERT_FALSE(buffer.Set(buffer.GetLastSeq() - 11, 0));
}

TEST(TestCircularBuffer, Wrap)
{
	CircularBuffer<uint32_t, uint32_t, 32768> buffer;

	ASSERT_TRUE(buffer.Set(-10, 1));
	//Log("%i %i %d\n", buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());

	ASSERT_TRUE(buffer.IsPresent(-10));
	ASSERT_EQ(buffer.Get(-10), 1);
	ASSERT_EQ(buffer.GetFirstSeq(), -10);
	ASSERT_EQ(buffer.GetLastSeq(), -10);


	ASSERT_TRUE(buffer.Set(1, 2));
	//Log("%i %i %d\n", buffer.GetFirstSeq(), buffer.GetLastSeq(), buffer.GetLength());
	ASSERT_TRUE(buffer.IsPresent(1));
	ASSERT_EQ(buffer.Get(1), 2);
	ASSERT_EQ(buffer.GetFirstSeq(), -10);
	ASSERT_EQ(buffer.GetLastSeq(), 1);

}