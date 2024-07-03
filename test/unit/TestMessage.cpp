#include <gtest/gtest.h>

#include "message.h"

using namespace data;

void CheckEqual(const Message& a, const Message& b)
{
	ASSERT_EQ(a.GetInfo().type, b.GetInfo().type);
	ASSERT_EQ(a.GetInfo().port, b.GetInfo().port);
	ASSERT_EQ(a.GetInfo().streamId, b.GetInfo().streamId);
	
	ASSERT_EQ(a.GetLength(), b.GetLength());
	ASSERT_EQ(0, memcmp(a.GetData(), b.GetData(), b.GetLength()));
}

void CheckDefault(const Message& msg)
{
	ASSERT_EQ(MessageType::Unkown, msg.GetInfo().type);
	ASSERT_EQ(0, msg.GetInfo().port);
	ASSERT_EQ(0, msg.GetInfo().streamId);
	ASSERT_EQ(nullptr, msg.GetData());
	ASSERT_EQ(0, msg.GetLength());
}


TEST(TestMessage, Constructors)
{
	{
		Message msg;
		CheckDefault(msg);
	}
	
	{
		Message msg(MessageType::Scte35, 1, 1);
		ASSERT_EQ(1, msg.GetInfo().port);
		ASSERT_EQ(1, msg.GetInfo().streamId);
		
		uint8_t testData[8];
		memset(testData, 0x5, sizeof(testData));
		msg.SetData(testData, sizeof(testData));
		ASSERT_NE(nullptr, msg.GetData());
		ASSERT_EQ(sizeof(testData), msg.GetLength());
		ASSERT_EQ(0, memcmp(testData, msg.GetData(), sizeof(testData)));
		
		// Copy constructor
		Message msg2(msg);
		ASSERT_NO_FATAL_FAILURE(CheckEqual(msg, msg2));
		
		Message msg3;
		msg3 = msg;
		ASSERT_NO_FATAL_FAILURE(CheckEqual(msg, msg3));
		
		Message msg4(std::move(msg2));
		ASSERT_NO_FATAL_FAILURE(CheckEqual(msg, msg4));
		ASSERT_NO_FATAL_FAILURE(CheckDefault(msg2));
		
		Message msg5;
		msg5 = std::move(msg3);
		ASSERT_NO_FATAL_FAILURE(CheckEqual(msg, msg5));
		ASSERT_NO_FATAL_FAILURE(CheckDefault(msg3));
	}
}