#include "PacketHeader.h"

// checksum is a one's complement, so it's independent of endianness

void PacketHeader::CalculateIpChecksum(PacketHeader& header)
{
	header.ip.hdrChecksum = 0;
	uint32_t checksum = 0;

	for (size_t i = 0; i < (sizeof(header.ip_hws) / sizeof(*header.ip_hws)); i++)
		checksum += header.ip_hws[i];

	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	header.ip.hdrChecksum = ~checksum;
}

void PacketHeader::CalculateUdpChecksum(PacketHeader& header, const Packet& payload)
{
	union {
		UdpIpv4PseudoHeader hdr;
		uint16_t words [sizeof(UdpIpv4PseudoHeader) / 2];
	} udpPhdr;
	udpPhdr.hdr = { header.ip.src, header.ip.dst, 0, header.ip.protocol, header.udp.length };

	header.udp.checksum = 0;
	uint32_t checksum = 0;

	for (size_t i = 0; i < (sizeof(udpPhdr.words) / sizeof(*udpPhdr.words)); i++)
		checksum += udpPhdr.words[i];
	for (size_t i = 0; i < (sizeof(header.udp_hws) / sizeof(*header.udp_hws)); i++)
		checksum += header.udp_hws[i];

	// FIXME: make sure data is aligned and we're not breaking strict aliasing
	uint16_t* data_words = (uint16_t*) payload.GetData();
	size_t data_nwords = payload.GetSize() / 2;
	for (size_t i = 0; i < data_nwords; i++)
		checksum += data_words[i];
	if (payload.GetSize() % 2 != 0)
		checksum += uint16_t(payload.GetData()[payload.GetSize() - 1]);

	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	header.udp.checksum = ~checksum;
	if (!header.udp.checksum) header.udp.checksum = 0xFFFF;
}
