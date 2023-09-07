#include "TestCommon.h"

#include "config.h"
#include "log.h"
#include "rtp.h"
#include "rtp/RTPHeaderExtension.h"
#include <algorithm>
#include <array>


TEST(TestVideoLayersAllocation, CanWriteAndParseLayersAllocationWithZeroSpatialLayers)
{
	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	//Empty layer
	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();

	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);

	EXPECT_GE(parsed.Parse(extMap, buffer.data(), len), 0);
	EXPECT_TRUE(parsed.hasVideoLayersAllocation);
	EXPECT_TRUE(parsed.videoLayersAllocation.has_value());
	EXPECT_EQ(extension.videoLayersAllocation, parsed.videoLayersAllocation.value());
}

TEST(TestVideoLayersAllocation, CanWriteAndParse2SpatialWith2TemporalLayers)
{
	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();

	extension.videoLayersAllocation->streamIdx = 1;
	extension.videoLayersAllocation->numRtpStreams = 2;
	extension.videoLayersAllocation->activeSpatialLayers = 
	{
		{
			/*streamIdx*/ 0,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ {25, 50},
		},
		{
			/*streamIdx*/ 1,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ {100, 200},
		},
	};
	
	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);

	//Parse
	EXPECT_GE(parsed.Parse(extMap, buffer.data(), len), 0);
	EXPECT_TRUE(parsed.hasVideoLayersAllocation);
	EXPECT_TRUE(parsed.videoLayersAllocation.has_value());
	EXPECT_EQ(extension.videoLayersAllocation, parsed.videoLayersAllocation.value());
}


TEST(TestVideoLayersAllocation, CanWriteAndParseAllocationWithDifferentNumerOfSpatialLayers) 
{
	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();

	extension.videoLayersAllocation->streamIdx = 1;
	extension.videoLayersAllocation->numRtpStreams = 2;
	extension.videoLayersAllocation->activeSpatialLayers =
	{
		{
			/*streamIdx*/ 0,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{50,},
		},
		{
			/*streamIdx*/ 1,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{100,},
		},
		{
			/*streamIdx*/ 1,
			/*spatialId*/ 1,
			/*targetBitratePerTemporalLayer*/std::vector<uint16_t>{200,},
		},
	};
	
	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);

	//Parse
	EXPECT_GE(parsed.Parse(extMap, buffer.data(), len), 0);
	EXPECT_TRUE(parsed.hasVideoLayersAllocation);
	EXPECT_TRUE(parsed.videoLayersAllocation.has_value());
	EXPECT_EQ(extension.videoLayersAllocation, parsed.videoLayersAllocation.value());
}


TEST(TestVideoLayersAllocation, CanWriteAndParseAllocationWithSkippedLowerSpatialLayer)
{
	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();

	extension.videoLayersAllocation->streamIdx = 1;
	extension.videoLayersAllocation->numRtpStreams = 2;
	extension.videoLayersAllocation->activeSpatialLayers =
	{
		{
			/*streamIdx*/ 0,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{50,},
		},
		{
			/*streamIdx*/ 1,
			/*spatialId*/ 1,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{200,},
		},
	};
	
	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);

	//Parse
	EXPECT_GE(parsed.Parse(extMap, buffer.data(), len), 0);
	EXPECT_TRUE(parsed.hasVideoLayersAllocation);
	EXPECT_TRUE(parsed.videoLayersAllocation.has_value());
	EXPECT_EQ(extension.videoLayersAllocation, parsed.videoLayersAllocation.value());
}


TEST(TestVideoLayersAllocation,	CanWriteAndParseAllocationWithSkippedRtpStreamIds) 
{
	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();

	extension.videoLayersAllocation->streamIdx = 2;
	extension.videoLayersAllocation->numRtpStreams = 3;
	extension.videoLayersAllocation->activeSpatialLayers =
	{
		{
			/*streamIdx*/ 0,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{50,},
		},
		{
			/*streamIdx*/ 2,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{200,},
		},
	};
	
	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);

	//Parse
	EXPECT_GE(parsed.Parse(extMap, buffer.data(), len), 0);
	EXPECT_TRUE(parsed.hasVideoLayersAllocation);
	EXPECT_TRUE(parsed.videoLayersAllocation.has_value());
	EXPECT_EQ(extension.videoLayersAllocation, parsed.videoLayersAllocation.value());

}


TEST(TestVideoLayersAllocation, CanWriteAndParseAllocationWithDifferentNumerOfTemporalLayers)
{

	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();

	extension.videoLayersAllocation->streamIdx = 1;
	extension.videoLayersAllocation->numRtpStreams = 3;
	extension.videoLayersAllocation->activeSpatialLayers =
	{
		{
			/*streamIdx*/ 0,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{25, 50,},
		},
		{
			/*streamIdx*/ 1,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{100,},
		},
	};
	
	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);
}


TEST(TestVideoLayersAllocation, CanWriteAndParseAllocationWithResolution)
{
	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();

	extension.videoLayersAllocation->streamIdx = 1;
	extension.videoLayersAllocation->numRtpStreams = 3;
	extension.videoLayersAllocation->activeSpatialLayers =
	{
		{
			/*streamIdx*/ 0,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{25, 50,},
			/*width*/ 320,
			/*height*/ 240,
			/*frame_rate_fps*/ 8,
		},
		{
			/*streamIdx*/ 1,
			/*spatialId*/ 0,
			/*targetBitratePerTemporalLayer*/ std::vector<uint16_t>{200,},
			/*width*/ 640,
			/*height*/ 320,
			/*frame_rate_fps*/ 30,
		},
	};
	
	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);
}

TEST(TestVideoLayersAllocation, WriteEmptyAllocationCanHaveAnyRtpStreamIndex) 
{
	RTPMap	extMap;
	extMap.SetCodecForType(RTPHeaderExtension::VideoLayersAllocation, RTPHeaderExtension::VideoLayersAllocation);

	std::array<uint8_t, MTU> buffer = {};

	RTPHeaderExtension extension;
	RTPHeaderExtension parsed;

	//Empty layer
	extension.hasVideoLayersAllocation = true;
	extension.videoLayersAllocation.emplace();
	extension.videoLayersAllocation->streamIdx = 1;

	//Serialize
	int len = extension.Serialize(extMap, buffer.data(), buffer.size());

	EXPECT_GE(len, 0);
}
