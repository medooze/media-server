
#include "TestCommon.h"
#include "rtmp/amf.h"
#include <limits>

TEST(TestAMFNumber, TestAMFNumberParsing)
{
	{
		// Small number
		AMFNumber number;
		std::vector<uint8_t> data = {0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(1.0, number.GetNumber());
	}
	
	{
		// Small number
		AMFNumber number;
		std::vector<uint8_t> data = {0x3f, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(0.01171875, number.GetNumber());
	}
	
	{
		// Small number
		AMFNumber number;
		std::vector<uint8_t> data = {0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(1.0000000000000004, number.GetNumber());
	}
	
	
	{
		// Big number
		AMFNumber number;
		std::vector<uint8_t> data = {0x7E, 0x45, 0x79, 0x8E, 0xE2, 0x30, 0x8C, 0x39};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(1.7976931348623157e+300, number.GetNumber());
	}
	
	
	{
		// Close to zero
		AMFNumber number;
		std::vector<uint8_t> data = {0x01, 0xB7, 0xD7, 0x84, 0x00, 0x00, 0x00, 0x00};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(2.2250738585072014e-300, number.GetNumber());
	}	

	{
		// Negative number close to zero
		AMFNumber number;
		std::vector<uint8_t> data = {0x81, 0xB7, 0xD7, 0x84, 0x00, 0x00, 0x00, 0x00};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(-2.2250738585072014e-300, number.GetNumber());
	}
	
	{
		// Min (positive) number
		AMFNumber number;
		std::vector<uint8_t> data = {0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(std::numeric_limits<double>::min(), number.GetNumber());
	}
	
	{
		// Max number
		AMFNumber number;
		std::vector<uint8_t> data = {0x7F, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(std::numeric_limits<double>::max(), number.GetNumber());
	}
	
	{
		// Lowest (negative) number
		AMFNumber number;
		std::vector<uint8_t> data = {0xFF, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(std::numeric_limits<double>::lowest(), number.GetNumber());
	}
	
	{
		// The exponent part max is 0x7FE, otherwise 0 is returned
		AMFNumber number;
		std::vector<uint8_t> data = {0x7F, 0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(0, number.GetNumber());
	}
	
	{
		// The exponent part max is 0x7FE, otherwise 0 is returned
		AMFNumber number;
		std::vector<uint8_t> data = {0xFF, 0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1};	
		ASSERT_EQ(8, number.Parse(data.data(), data.size()));
		EXPECT_EQ(0, number.GetNumber());
	}
}