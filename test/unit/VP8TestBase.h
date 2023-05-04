#ifndef VP8_TEST_BASE_H
#define VP8_TEST_BASE_H

#include "rtp.h"
#include "TestCommon.h"
#include <cstdlib>
#include <limits>

class VP8TestBase : public ::testing::Test
{
public:

	static constexpr uint32_t TEST_SSRC = 0x1234;

	VP8TestBase(uint16_t picId = 0, uint8_t tl0PicId = 0) :
		currentPicId(picId),
		currentTl0PicId(tl0PicId)
	{
	}

	std::shared_ptr<RTPPacket> StartPacket(uint64_t tm, uint8_t pid, uint8_t layerIdx = 0)
	{
		return CreatePacket(tm, currentSeqNum++, true, pid, false, layerIdx);
	}

	std::shared_ptr<RTPPacket> MiddlePacket(uint64_t tm, uint8_t pid, uint8_t layerIdx = 0)
	{
		return CreatePacket(tm, currentSeqNum++, false, pid, false, layerIdx);
	}

	std::shared_ptr<RTPPacket> MarkerPacket(uint64_t tm, uint8_t pid, uint8_t layerIdx = 0)
	{
		return CreatePacket(tm, currentSeqNum++, false, pid, true, layerIdx);
	}

protected:

	std::shared_ptr<RTPPacket> CreatePacket(uint64_t tm, uint32_t seqnum, bool startOfPartition, uint8_t pid, bool mark, uint8_t layerIdx)
	{
		std::shared_ptr<RTPPacket> packet = std::make_shared<RTPPacket>(MediaFrame::Type::Video, VideoCodec::VP8);

		packet->SetExtTimestamp(tm);
		packet->SetSSRC(TEST_SSRC);
		packet->SetExtSeqNum(seqnum);
		packet->SetMark(mark);

		constexpr size_t BufferSize = 1024;
		unsigned char buffer[BufferSize];
		memset(buffer, 0, BufferSize);

		VP8PayloadDescriptor desc(startOfPartition, pid);

		if (currentTimestamp != tm)
		{
			currentPicId++;

			if (layerIdx == 0)
			{
				currentTl0PicId++;
			}

			currentTimestamp = tm;
		}

		desc.pictureIdPresent = true;
		desc.pictureId = currentPicId;

		desc.temporalLayerIndexPresent = true;
		desc.temporalLayerIndex = layerIdx;

		desc.temporalLevelZeroIndexPresent = true;
		desc.temporalLevelZeroIndex = currentTl0PicId;
		desc.layerSync = layerIdx != 0;

		packet->vp8PayloadDescriptor = desc;

		auto len = desc.Serialize(buffer, BufferSize);

		VP8PayloadHeader header;
		header.isKeyFrame = layerIdx == 0;
		header.showFrame = true;
		packet->vp8PayloadHeader = header;

		packet->SetPayload(buffer, BufferSize);
		packet->AdquireMediaData();

		return packet;
	}

	uint64_t currentTimestamp = std::numeric_limits<uint64_t>::max();
	uint32_t currentSeqNum = 0;
	uint16_t currentPicId = 0;
	uint8_t currentTl0PicId = 0;
};

#endif
