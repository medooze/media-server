#include "TestCommon.h"

#include "Packetizer.h"

template<size_t N>
struct TestMessage : public Serializable
{
	TestMessage()
	{
		for (size_t i = 0; i < N; i++)
		{
			data[i] = i;
		}
	}
	
	void Serialize(BufferWritter& writer) const override
	{
		writer.SetN(data, N);
	}
	
	size_t GetSize() const override
	{
		return N;
	}
	
	std::array<uint8_t, N> data;
};

void CheckNextPacketSize(size_t maxPacketSize, size_t expectedNextPacketSize, Packetizer& packetizer, 
			std::optional<std::vector<uint8_t>> expectedPacketContent = std::nullopt)
{
	std::vector<uint8_t> packet(maxPacketSize);
	BufferWritter writer(packet.data(), maxPacketSize);
	ASSERT_EQ(expectedNextPacketSize, packetizer.WriteNextPacket(writer));
	
	if (expectedPacketContent)
	{
		ASSERT_EQ(expectedNextPacketSize, expectedPacketContent->size());
		packet.resize(expectedNextPacketSize);
		ASSERT_EQ(*expectedPacketContent, packet);
	}
}

TEST(TestPacketizer, TestForceSeparate)
{
	Packetizer packetizer(100);
	
	packetizer.AddMessage(std::make_shared<TestMessage<12>>());
	packetizer.AddMessage(std::make_shared<TestMessage<16>>());
	packetizer.AddMessage(std::make_shared<TestMessage<18>>());
	
	ASSERT_TRUE(packetizer.HasData());
	
	ASSERT_TRUE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer, {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}}));
	
	ASSERT_FALSE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 2, packetizer, {{10, 11}}));
	
	ASSERT_TRUE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer, {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}}));
	
	ASSERT_FALSE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 6, packetizer, {{10, 11, 12, 13, 14, 15}}));
	
	ASSERT_TRUE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer, {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}}));
	
	ASSERT_FALSE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 8, packetizer, {{10, 11, 12, 13, 14, 15, 16, 17}}));
	
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 0, packetizer));
	ASSERT_FALSE(packetizer.HasData());
}

TEST(TestPacketizer, TestNoForceSeparate)
{
	Packetizer packetizer(100);
	
	packetizer.AddMessage(std::make_shared<TestMessage<12>>(), false);
	packetizer.AddMessage(std::make_shared<TestMessage<16>>(), false);
	packetizer.AddMessage(std::make_shared<TestMessage<18>>(), false);
	
	ASSERT_TRUE(packetizer.HasData());
	
	ASSERT_TRUE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer, {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}}));
	
	ASSERT_FALSE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer, {{10, 11, 0, 1, 2, 3, 4, 5, 6, 7}}));
	
	ASSERT_FALSE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer, {{8, 9, 10, 11, 12, 13, 14, 15, 0, 1}}));
	
	ASSERT_FALSE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer, {{2, 3, 4, 5, 6, 7, 8, 9, 10, 11}}));
	
	ASSERT_FALSE(packetizer.IsMessageStart());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 6, packetizer, {{12, 13, 14, 15, 16, 17}}));
	
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 0, packetizer));
	ASSERT_FALSE(packetizer.HasData());
}

TEST(TestPacketizer, TestMixed)
{
	Packetizer packetizer(100);
	
	packetizer.AddMessage(std::make_shared<TestMessage<12>>(), false);
	packetizer.AddMessage(std::make_shared<TestMessage<16>>(), true);
	packetizer.AddMessage(std::make_shared<TestMessage<18>>(), false);
	
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 2, packetizer));
	
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 4, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 0, packetizer));
}

TEST(TestPacketizer, TestMessageEdgeMatchPacket)
{
	Packetizer packetizer(100);
	
	packetizer.AddMessage(std::make_shared<TestMessage<10>>());
	packetizer.AddMessage(std::make_shared<TestMessage<20>>());
	packetizer.AddMessage(std::make_shared<TestMessage<30>>());
	
	ASSERT_TRUE(packetizer.HasData());
	
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 0, packetizer));
	ASSERT_FALSE(packetizer.HasData());
}

TEST(TestPacketizer, TestDropMessage)
{
	Packetizer packetizer(2);
	
	packetizer.AddMessage(std::make_shared<TestMessage<12>>(), false);
	packetizer.AddMessage(std::make_shared<TestMessage<16>>(), true);
	packetizer.AddMessage(std::make_shared<TestMessage<18>>(), false);
	
	// Note first message got dropped
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 10, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 4, packetizer));
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 0, packetizer));
}

TEST(TestPacketizer, TestZeroSizeMessage)
{
	Packetizer packetizer(2);
	
	packetizer.AddMessage(std::make_shared<TestMessage<0>>(), false);
	
	ASSERT_TRUE(packetizer.HasData());
	ASSERT_NO_FATAL_FAILURE(CheckNextPacketSize(10, 0, packetizer));
	ASSERT_FALSE(packetizer.HasData());
}
