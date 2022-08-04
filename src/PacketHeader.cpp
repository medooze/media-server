#include "PacketHeader.h"

#include <cctype>
#include <stdexcept>

using MacAddr = PacketHeader::MacAddr;

MacAddr PacketHeader::ParseMac(std::string str)
{
	if (str.length() != 17)
		throw std::invalid_argument("invalid MAC address: incorrect length");
	auto HexDigit = [](unsigned char c) {
		static std::string digits = "0123456789abcdef";
		if (auto pos = digits.find(std::tolower(c)); pos != std::string::npos)
			return pos;
		throw std::invalid_argument("invalid MAC address: incorrect hex digit");
	};
	MacAddr result;
	for (size_t i = 0; i < 6; i++)
	{
		result[i] = HexDigit(str[i * 3 + 0]) << 4 | HexDigit(str[i * 3 + 1]);
		if (i < 5 && str[i * 3 + 2] != ':')
			throw std::invalid_argument("invalid MAC address: incorrect separator");
	}
	return result;
}


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

PacketHeader PacketHeader::Create(const MacAddr& selfLladdr, uint16_t port)
{
	PacketHeader header;
	memset(&header, 0, sizeof(header));
	memcpy(header.eth.ether_shost, selfLladdr.data(), 6);
	header.eth.ether_type = htons(ETHERTYPE_IP);
	header.ip.ihl = sizeof(header.ip) / 4;
	header.ip.version = 4;
	header.ip.tos = 0x2E;
	header.ip.flags = 0;
	header.ip.fragOffset = 0;
	header.ip.ttl = 64;
	header.ip.protocol = IPPROTO_UDP;
	header.udp.src = htons(port);
	return header;
}

thread_local uint16_t identificationCounter = 0;

void PacketHeader::PrepareHeader(PacketHeader& header, uint32_t ip, uint16_t port, const CandidateData& rawTxData, const Packet& payload)
{
	memcpy(header.eth.ether_dhost, rawTxData.dstLladdr.data(), 6);
	header.ip.src = htonl(rawTxData.selfAddr);
	header.ip.dst = htonl(ip);
	header.udp.dst = htons(port);
	header.udp.length = htons(sizeof(header.udp) + payload.GetSize());
	header.ip.totalLen = htons((sizeof(header) - sizeof(header.eth)) + payload.GetSize());
	header.ip.identification = htons(identificationCounter++);
	PacketHeader::CalculateIpChecksum(header);
	PacketHeader::CalculateUdpChecksum(header, payload);
}
