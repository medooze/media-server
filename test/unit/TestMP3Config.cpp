#include "TestCommon.h"

#include "config.h"
#include "log.h"
#include "mp3/MP3Config.h"
#include <algorithm>

TEST(TestMP3Config, SupportedMP3Config)
{
	{
		BYTE mp3[4] = { 0xff, 0xfb, 0x74, 0xc4 };
		MP3Config config;
		ASSERT_TRUE(config.Decode(mp3, sizeof(mp3)));
		/*
		[MP3Config
			mpegVersion = 1
			mpegLayer = layerIII
			rate = 48000
			channels = 1
			paddingSize = 0
			frameLength = 1152
		/ ]
		*/
		ASSERT_EQ(config.GetAudioVersion()	, MP3Config::MPEGAudioVersion::MPEGVersion1);
		ASSERT_EQ(config.GetAudioLayer()	, MP3Config::MPEGAudioLayer::MPEGLayer3);
		ASSERT_EQ(config.GetRate()		, 48000);
		ASSERT_EQ(config.GetChannels()		, 1);
		ASSERT_EQ(config.GetFrameLength()	, 1152);
		ASSERT_EQ(config.GetBitrate()	, 96000);

	}
	{
		BYTE mp3[4] = { 0xff, 0xf3, 0x44, 0xc4 };
		MP3Config config;
		ASSERT_TRUE(config.Decode(mp3, sizeof(mp3)));
		/*
		[MP3Config
			mpegVersion = 2
			mpegLayer = layerIII
			rate = 24000
			channels = 1
			paddingSize = 0
			frameLength = 576
			bitrate = 96000
		/ ]
		*/
		ASSERT_EQ(config.GetAudioVersion()	, MP3Config::MPEGAudioVersion::MPEGVersion2);
		ASSERT_EQ(config.GetAudioLayer()	, MP3Config::MPEGAudioLayer::MPEGLayer3);
		ASSERT_EQ(config.GetRate()		, 24000);
		ASSERT_EQ(config.GetChannels()		, 1);
		ASSERT_EQ(config.GetFrameLength()	, 576);
		ASSERT_EQ(config.GetBitrate()	, 32000);
	}
	{
		BYTE mp3[4] = { 0xff, 0xfb, 0x70, 0x64 };
		MP3Config config;
		ASSERT_TRUE(config.Decode(mp3, sizeof(mp3)));
		/*
		[MP3Config
			mpegVersion = 1
			mpegLayer = layerIII
			rate = 44100
			channels = 2
			paddingSize = 0
			frameLength = 1152
			bitrate = 96000
		/ ]
		*/
		ASSERT_EQ(config.GetAudioVersion()	, MP3Config::MPEGAudioVersion::MPEGVersion1);
		ASSERT_EQ(config.GetAudioLayer()	, MP3Config::MPEGAudioLayer::MPEGLayer3);
		ASSERT_EQ(config.GetRate()		, 44100);
		ASSERT_EQ(config.GetChannels()		, 2);
		ASSERT_EQ(config.GetFrameLength()	, 1152);
		ASSERT_EQ(config.GetBitrate()	, 96000);
	}
}

TEST(TestMP3Config, UnsupportedMP3Config)
{
	{
		// mpeg version : mpeg2extension
		BYTE mp3[4] = { 0xff, 0xe3, 0x74, 0xc4 };
		MP3Config config;
		ASSERT_FALSE(config.Decode(mp3, sizeof(mp3)));
	}
	{
		// mpeg layer I
		BYTE mp3[4] = { 0xff, 0xf7, 0x44, 0xc4 };
		MP3Config config;
		ASSERT_FALSE(config.Decode(mp3, sizeof(mp3)));
	}
}
