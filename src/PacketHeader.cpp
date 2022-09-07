#include "PacketHeader.h"

#include "CheckSum.h"

void PacketHeader::CalculateIpChecksum()
{
	CheckSum checksum;
	
	this->ip.hdrChecksum = 0;
	checksum.Calculate((const char*)&this->ip, sizeof(this->ip));
	this->ip.hdrChecksum = checksum.Finalize();
}

void PacketHeader::CalculateUdpChecksum(const Packet& payload)
{
	CheckSum checksum;
	UdpIpv4PseudoHeader udpPhdr = { this->ip.src, this->ip.dst, 0, this->ip.protocol, this->udp.length };
	
	this->udp.checksum = 0;
	checksum.Calculate((const char*)&udpPhdr, sizeof(udpPhdr));
	checksum.Calculate((const char*)&this->udp, sizeof(this->udp));
	checksum.Calculate((const char*)payload.GetData(), payload.GetSize());
	this->udp.checksum = checksum.Finalize();
	// in UDP specifically, a zeroed checksum means "no checksum"
	// and we must set FFFF instead:
	if (!this->udp.checksum) this->udp.checksum = 0xFFFF;
}

PacketHeader PacketHeader::Create(const MacAddress& selfLladdr, uint16_t port)
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

void PacketHeader::PrepareHeader(PacketHeader& header, uint32_t ip, uint16_t port, const FlowRoutingInfo& rawTxData, const Packet& payload)
{
	memcpy(header.eth.ether_dhost, rawTxData.dstLladdr.data(), 6);
	header.ip.src = htonl(rawTxData.selfAddr);
	header.ip.dst = htonl(ip);
	header.udp.dst = htons(port);
	header.udp.length = htons(sizeof(header.udp) + payload.GetSize());
	header.ip.totalLen = htons((sizeof(header) - sizeof(header.eth)) + payload.GetSize());
	header.ip.identification = htons(identificationCounter++);
	header.CalculateIpChecksum();
	header.CalculateUdpChecksum(payload);
}
