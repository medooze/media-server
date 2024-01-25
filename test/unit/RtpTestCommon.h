#ifndef RTP_TEST_COMMON_H
#define RTP_TEST_COMMON_H

#include "rtp/RTPMap.h"
#include "rtp/RTPPacket.h"

constexpr int8_t kPayloadType = 100; //0x64

constexpr WORD kSeqNum = 0x1234;
constexpr BYTE kSeqNumFirstByte = kSeqNum >> 8;
constexpr BYTE kSeqNumSecondByte = kSeqNum & 0xFF;

constexpr DWORD kSsrc = 0x12345678;
constexpr BYTE kSsrcFirstByte = kSsrc >> 24;
constexpr BYTE kSsrcSecondByte = (kSsrc >> 16) & 0xFF;
constexpr BYTE kSsrcThirdByte = (kSsrc >> 8) & 0xFF;
constexpr BYTE kSsrcFourthdByte = kSsrc & 0xFF;

constexpr DWORD kTimeStamp = 0x11223344;
constexpr BYTE kTimeStampFirstByte = kTimeStamp >> 24;
constexpr BYTE kTimeStampSecondByte = (kTimeStamp >> 16) & 0xFF;
constexpr BYTE kTimeStampThirdByte = (kTimeStamp >> 8) & 0xFF;
constexpr BYTE kTimeStampFourthdByte = kTimeStamp & 0xFF;

class RTPPacketGenerator
{
public:
	RTPPacketGenerator(const RTPMap& rtpMap, const RTPMap& extMap) :
		rtpMap(rtpMap),
		extMap(extMap)
	{}

	RTPPacket::shared Create(QWORD seqNumIncrement = 0, size_t mediaLength = 16)
	{
		DWORD extSeqNum = kSeqNum + seqNumIncrement;
		WORD seqCycles = extSeqNum >> 16;

		BYTE kSeqNumFirstByte = static_cast<WORD>(extSeqNum) >> 8;
		BYTE kSeqNumSecondByte = static_cast<WORD>(extSeqNum) & 0xff;
		std::vector<BYTE> kMinimumPacket =
		{
			0x80, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
			kTimeStampFirstByte, kTimeStampSecondByte, kTimeStampThirdByte, kTimeStampFourthdByte,
			kSsrcFirstByte, kSsrcSecondByte, kSsrcThirdByte, kSsrcFourthdByte,
		};

		BYTE payloadBytes[] =
		{
			'm', 'e', 'd', 'i',
			'a', 'p', 'a', 'y',
			'l', 'o', 'a', 'd'};

		size_t j = 0;
		for (size_t i = 0; i < mediaLength; ++i)
		{
			kMinimumPacket.push_back(payloadBytes[j]);
			j = ++j >= sizeof(payloadBytes) ? 0 : j;
		}

		auto packet = RTPPacket::Parse(kMinimumPacket.data(), kMinimumPacket.size(), rtpMap, extMap);
		packet->SetSeqCycles(seqCycles);
		return packet;
	}

private:
	const RTPMap& rtpMap;
	const RTPMap& extMap;
};

#endif // RTP_TEST_COMMON_H