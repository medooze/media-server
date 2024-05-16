#include "TestCommon.h"

#include "config.h"
#include "log.h"
#include "aac/aacconfig.h"
#include <algorithm>

TEST(TestAAC, AACSpecificConfig)
{
	BYTE aux[7] = {};
	size_t len;

	{
		BYTE aac[2] = { 0x16, 0x08 };
		AACSpecificConfig config;
		ASSERT_TRUE(config.Decode(aac, sizeof(aac)));
		/*
		[AACSpecificConfig
			objectType = 2
			rate = 7350
			channels = 1
			frameLength = 0
			coreCoder = 0
			coreCoderDelay = 0
			extension = 0
		/ ]
		*/
		ASSERT_EQ(config.GetObjectType()	, 2);
		ASSERT_EQ(config.GetRate()		, 7350);
		ASSERT_EQ(config.GetChannels()		, 1);
		ASSERT_EQ(config.GetFrameLength()	, 1024);
		ASSERT_EQ(config.Serialize(aux, sizeof(aux)), sizeof(aac));
		ASSERT_EQ(memcmp(aac,aux,sizeof(aac)), 0);
	}

	{
		BYTE aac[2] = { 0x11, 0x88 };
		AACSpecificConfig config;
		ASSERT_TRUE(config.Decode(aac, sizeof(aac)));
		/*
		[AACSpecificConfig
			objectType=2
			rate=48000
			channels=1
			frameLength=0
			coreCoder=0
			coreCoderDelay=0
			extension=0
		/]
		*/
		ASSERT_EQ(config.GetObjectType()	, 2);
		ASSERT_EQ(config.GetRate()		, 48000);
		ASSERT_EQ(config.GetChannels()		, 1);
		ASSERT_EQ(config.GetFrameLength()	, 1024);
		ASSERT_EQ(config.Serialize(aux, sizeof(aux)), sizeof(aac));
		ASSERT_EQ(memcmp(aac, aux, sizeof(aac)), 0);
	}
	 
	{
		BYTE aac[2] = { 0x11, 0x88 };
		AACSpecificConfig config(48000, 1);
		ASSERT_EQ(config.Serialize(aux, sizeof(aux)), sizeof(aac));
		ASSERT_EQ(memcmp(aac, aux, sizeof(aac)), 0);
	}

	{
		//With syncExtensionType
		/*AudioSpecificConfig:
		audioObjectType          0b00010 = 2
			samplingFrequenctyIndex  0b0011 = 3
			changelConfiguration     0b0010 = 2
			GASpecificConfig :
			frameLengthFlag         0
			dependOnCoreCoder       0
			extensionFlag           0
			syncExtensionType        0b0101 0110 111 = 0x2b7
			extensionAudioObjectType 0b00101 = 0x5
			sbrPresentFlag           0
		*/
		BYTE aac[5] = { 0x11, 0x90, 0x56, 0xe5, 0x00 };
		AACSpecificConfig config;
		ASSERT_TRUE(config.Decode(aac, sizeof(aac)));
		config.Dump();
		/*
		[AACSpecificConfig
			objectType=2
			rate=48000
			channels=2
			frameLength=0
			coreCoder=0
			coreCoderDelay=0
			extension=0
		/]
		*/
		ASSERT_EQ(config.GetObjectType(), 2);
		ASSERT_EQ(config.GetRate(), 48000);
		ASSERT_EQ(config.GetChannels(), 2);
		ASSERT_EQ(config.GetFrameLength(), 1024);
		ASSERT_EQ(config.Serialize(aux, sizeof(aux)), 2);
		ASSERT_EQ(memcmp(aac, aux, 2), 0);
	}
}
