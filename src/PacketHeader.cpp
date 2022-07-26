#include "PacketHeader.h"

// checksum is a one's complement, so it's independent of endianness

static void ChecksumUpdateWords(uint32_t& checksum, void* data, size_t nWords) {
	// memcpy is the only safe way to do type punning in C++ pre C++20. the compiler
	// should see the memcpy call to an identically sized type and optimize it away
	uint16_t words [nWords];
	memcpy(words, data, sizeof(words));
	for (size_t i = 0; i < nWords; i++)
		checksum += words[i];
}

static uint16_t ChecksumFinish(uint32_t checksum) {
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	return ~checksum;
}

void PacketHeader::CalculateIpChecksum(PacketHeader& header)
{
	header.ip.hdrChecksum = 0;
	uint32_t checksum = 0;
	ChecksumUpdateWords(checksum, &header.ip, sizeof(header.ip) / 2);
	header.ip.hdrChecksum = ChecksumFinish(checksum);
}

void PacketHeader::CalculateUdpChecksum(PacketHeader& header, const Packet& payload)
{
	UdpIpv4PseudoHeader udpPhdr = { header.ip.src, header.ip.dst, 0, header.ip.protocol, header.udp.length };

	header.udp.checksum = 0;
	uint32_t checksum = 0;

	ChecksumUpdateWords(checksum, &udpPhdr, sizeof(udpPhdr) / 2);
	ChecksumUpdateWords(checksum, &header.udp, sizeof(header.udp) / 2);
	ChecksumUpdateWords(checksum, payload.GetData(), payload.GetSize() / 2);
	if (payload.GetSize() % 2 != 0)
		checksum += uint16_t(payload.GetData()[payload.GetSize() - 1]);

	header.udp.checksum = ChecksumFinish(checksum);
	if (!header.udp.checksum) header.udp.checksum = 0xFFFF;
}
