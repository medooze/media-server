#include "ForwardErrorCorrection.h"
#include "media.h"
#include "rtp/RTPPacket.h"
#include "tools.h"
#include <arpa/inet.h>

namespace
{

constexpr BYTE kRtpHeaderFixedPartSize = 12;

// Flexfec03 implementation only support single-stream protection.
constexpr BYTE kSsrcCount = 1;

// There are three reserved bytes that MUST be set to zero in the header.
constexpr DWORD kReservedBits = 0;

// Size (in bytes) of part of header which is not packet mask specific.
constexpr size_t kBaseHeaderSize = 12;
// Size (in bytes) of part of header which is stream specific.
constexpr size_t kStreamSpecificHeaderSize = 6;
constexpr size_t kPacketMaskOffset = kBaseHeaderSize + kStreamSpecificHeaderSize;

// Size (in bits) of packet masks, given number of K bits set.
constexpr size_t kFlexfecPacketMaskSizesInBits[] = {15, 46, 109};
//
// Size (in bytes) of packet masks, given number of K bits set.
constexpr size_t kFlexfecPacketMaskSizesInBytes[] = {2, 6, 14};

// Size (in bytes) of header, given the single stream packet mask size, i.e.
// the number of K-bits set.
constexpr size_t kHeaderSizes[] =
{
	kBaseHeaderSize + kStreamSpecificHeaderSize + kFlexfecPacketMaskSizesInBytes[0],
	kBaseHeaderSize + kStreamSpecificHeaderSize + kFlexfecPacketMaskSizesInBytes[1],
	kBaseHeaderSize + kStreamSpecificHeaderSize + kFlexfecPacketMaskSizesInBytes[2]
};

constexpr BYTE kFecHeader[] =
{
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

void SetMaskBit(std::vector<BYTE>& mask, QWORD bit)
{
	QWORD byte = bit / 8;
	BYTE bitInByte = bit % 8;
	mask[byte] |= (1 << (7 - bitInByte));
}

BYTE GetMaskBit(std::vector<BYTE>& mask, QWORD bit)
{
	QWORD byte = bit / 8;
	BYTE bitInByte = bit % 8;
	return ((mask[byte] & (1 << (7 - bitInByte))) >> (7 - bitInByte));
}

}

size_t GetFlexfecHeaderSize(size_t packetMaskSize)
{
	if (packetMaskSize <= kFlexfecPacketMaskSizesInBits[0])
	{
		return kHeaderSizes[0];
	}
	else if (packetMaskSize <= kFlexfecPacketMaskSizesInBits[1])
	{
		return kHeaderSizes[1];
	}
	return kHeaderSizes[2];
}

void XorHeaders(BYTE* srcData, BYTE* dstData, DWORD srcDataSize)
{
	// XOR the first 2 bytes of the header: V, P, X, CC, M, PT fields.
	dstData[0] ^= srcData[0];
	dstData[1] ^= srcData[1];

	// XOR the length recovery field.
	BYTE srcPayloadLengthNetworkOrder[2];
	set2(srcPayloadLengthNetworkOrder, 0, srcDataSize);
	dstData[2] ^= srcPayloadLengthNetworkOrder[0];
	dstData[3] ^= srcPayloadLengthNetworkOrder[1];

	// XOR the 5th to 8th bytes of the header: the timestamp field.
	dstData[4] ^= srcData[4];
	dstData[5] ^= srcData[5];
	dstData[6] ^= srcData[6];
	dstData[7] ^= srcData[7];

	// Skip the 9th to 12th bytes of the header.
}

void XorPayloads(
	const BYTE* srcData,
	size_t payloadLength,
	size_t dstOffset,
	RTPPacket::shared dst)
{
	// XOR the payload.
	if (dstOffset + payloadLength > dst->GetMediaLength())
	{
		size_t oldSize = dst->GetMediaLength();
		size_t newSize = dstOffset + payloadLength;
		dst->SetMediaLength(newSize);
		memset(dst->AdquireMediaData() + oldSize, 0, newSize - oldSize);
	}
	BYTE* dstData = dst->AdquireMediaData();
	for (size_t i = 0; i < payloadLength; ++i)
	{
		dstData[dstOffset + i] ^= srcData[kRtpHeaderFixedPartSize + i];
	}
}

void FinalizeFecHeader(
	const DWORD mediaSsrc,
	const WORD seqNumBase,
	const std::vector<BYTE>& packetMask,
	BYTE packetMaskSize,
	RTPPacket::shared fecPacket)
{
	size_t currentHeaderSize = kHeaderSizes[2];
	size_t fecHeaderSize = GetFlexfecHeaderSize(packetMaskSize);

	// Move fixed header part to fit mask size
	if (currentHeaderSize - fecHeaderSize)
	{
		BYTE* data = fecPacket->AdquireMediaData();
		std::memmove(data + currentHeaderSize - fecHeaderSize, data, fecHeaderSize);
		fecPacket->SkipPayload(currentHeaderSize - fecHeaderSize);
	}

	BYTE* data = fecPacket->AdquireMediaData();

	data[0] &= 0x7f;  // Clear R bit.
	data[0] &= 0xbf;  // Clear F bit.

	set1(data, 8, kSsrcCount);
	set3(data, 9, kReservedBits);
	set4(data, 12, mediaSsrc);
	set2(data, 16, seqNumBase);

	// We treat the mask parts as unsigned integers with host order endianness
	// in order to simplify the bit shifting between bytes.
	BYTE* const writtenPacketMask = data + kPacketMaskOffset;

	if (packetMaskSize > kFlexfecPacketMaskSizesInBits[1])
	{
		// The packet mask is 109 bits long.
		WORD tmpMaskPart0 = get2(packetMask.data(), 0);
		DWORD tmpMaskPart1 = get4(packetMask.data(), 2);
		QWORD tmpMaskPart2 = get8(packetMask.data(), 6);

		tmpMaskPart0 >>= 1;  // Shift, thus clearing K-bit 0.
		set2(writtenPacketMask, 0, tmpMaskPart0);
		tmpMaskPart1 >>= 2;  // Shift, thus clearing K-bit 1 and bit 15.
		set4(writtenPacketMask, 2, tmpMaskPart1);
		tmpMaskPart2 >>= 3;  // Shift, thus clearing K-bit 2 and bits 46 and 47.
		set8(writtenPacketMask, 6, tmpMaskPart2);

		bool bit15 = (packetMask[1] & 0x01) != 0;
		if (bit15)
			writtenPacketMask[2] |= 0x40;  // Set bit 15.
		bool bit46 = (packetMask[5] & 0x02) != 0;
		bool bit47 = (packetMask[5] & 0x01) != 0;
		if (bit46)
			writtenPacketMask[6] |= 0x40;  // Set bit 46.
		if (bit47)
			writtenPacketMask[6] |= 0x20;  // Set bit 47.
		writtenPacketMask[6] |= 0x80;         // Set K-bit 2.
	}
	else if (packetMaskSize > kFlexfecPacketMaskSizesInBits[0])
	{
		// The packet mask is 46 bits long.
		WORD tmpMaskPart0 = get2(packetMask.data(), 0);
		DWORD tmpMaskPart1 = get4(packetMask.data(), 2);

		tmpMaskPart0 >>= 1;  // Shift, thus clearing K-bit 0.
		set2(writtenPacketMask, 0, tmpMaskPart0);
		tmpMaskPart1 >>= 2;  // Shift, thus clearing K-bit 1 and bit 15.
		set4(writtenPacketMask, 2, tmpMaskPart1);

		bool bit15 = (packetMask[1] & 0x01) != 0;
		if (bit15)
			writtenPacketMask[2] |= 0x40;  // Set bit 15.

		writtenPacketMask[2] |= 0x80;  // Set K-bit 1.

	}
	else
	{
		// The packet mask is 15 bits long.
		WORD tmpMaskPart0 = get2(packetMask.data(), 0);

		tmpMaskPart0 >>= 1;  // Shift, thus clearing K-bit 0.
		set2(writtenPacketMask, 0, tmpMaskPart0);
		writtenPacketMask[0] |= 0x80;  // Set K-bit 0.
	}
}

ForwardErrorCorrection::ForwardErrorCorrection() :
	maxAllowedMaskSize(kFlexfecPacketMaskSizesInBits[1]),
	mask(kFlexfecPacketMaskSizesInBytes[2], 0),
	buffer(MTU, 0) {}

RTPPacket::shared ForwardErrorCorrection::EncodeFec(
	const std::vector<RTPPacket::shared>& mediaPackets,
	const RTPMap& extMap)
{
	if (mediaPackets.empty())
		return nullptr;

	// Payload type will be overriden before the send
	auto fecPacket = std::make_shared<RTPPacket>(MediaFrame::Video, 49);
	fecPacket->AdquireMediaData();

	const DWORD mediaSsrc = mediaPackets.front()->GetSSRC();
	const DWORD extSeqNumBase = mediaPackets.front()->GetExtSeqNum();

	size_t fecHeaderSize = kHeaderSizes[2];
	WORD packetMaskSize = 0;
	fill(mask.begin(), mask.end(), 0);
	fecPacket->SetPayload(kFecHeader, fecHeaderSize);
	for (RTPPacket::shared mediaPacket : mediaPackets)
	{
		// We can only protect single SSRC
		if (mediaPacket->GetSSRC() != mediaSsrc) continue;
		// Mask can only fit maxAllowedMaskSize_ packets
		if (mediaPacket->GetExtSeqNum() - extSeqNumBase >= maxAllowedMaskSize) continue;
		// Skip duplicated media packets
		if (GetMaskBit(mask, mediaPacket->GetExtSeqNum() - extSeqNumBase)) continue;

		SetMaskBit(mask, mediaPacket->GetExtSeqNum() - extSeqNumBase);
		packetMaskSize = std::max(packetMaskSize, static_cast<WORD>(mediaPacket->GetExtSeqNum() - extSeqNumBase + 1));

		// TODO. Avoid media payload serialization
		DWORD len = mediaPacket->Serialize(buffer.data(), buffer.size(), extMap);

		XorHeaders(buffer.data(), fecPacket->AdquireMediaData(), len - kRtpHeaderFixedPartSize);
		XorPayloads(buffer.data(), len - kRtpHeaderFixedPartSize, fecHeaderSize, fecPacket);
	}

	FinalizeFecHeader(mediaSsrc, mediaPackets.front()->GetSeqNum(), mask, packetMaskSize, fecPacket);
	return fecPacket;
}
