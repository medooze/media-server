#ifndef _PACKETHEADER_H_
#define _PACKETHEADER_H_

#include <cstdint>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <array>
#include <string>
#include <random>
#include "Packet.h"

/**
 * @brief Provides structs and logic to build Ethernet packet headers
 */
struct PacketHeader {
	using MacAddr = std::array<uint8_t, 6>;

	/**
	 * @brief Parse an "xx:xx:xx:xx:xx:xx" string into a MacAddr
	 */
	static MacAddr ParseMac(std::string str);


	struct CandidateData {
		uint32_t selfAddr;
		MacAddr dstLladdr;
	};


	struct IpHeader {
		uint8_t ihl : 4;
		uint8_t version : 4;
		uint8_t tos;
		uint16_t totalLen;
		uint16_t identification;
		uint8_t flags : 3;
		uint16_t fragOffset : 13;
		uint8_t ttl;
		uint8_t protocol;
		uint16_t hdrChecksum;
		uint32_t src;
		uint32_t dst;
		// options
	} __attribute__ ((packed));

	struct UdpHeader {
		uint16_t src;
		uint16_t dst;
		uint16_t length;
		uint16_t checksum;
	} __attribute__ ((packed));

	struct UdpIpv4PseudoHeader {
		uint32_t src;
		uint32_t dst;
		uint8_t zero;
		uint8_t protocol;
		uint16_t udpLength;
	} __attribute__ ((packed));


	// DATA
	ether_header eth;
	IpHeader ip;
	UdpHeader udp;

	/**
	 * @brief Calculate and (re)set the IP header checksum
	 */
	static void CalculateIpChecksum(PacketHeader& header);

	/**
	 * @brief Calculate and (re)set the UDP checksum
	 */
	static void CalculateUdpChecksum(PacketHeader& header, const Packet& payload);

	/**
	 * @brief Initialize the frame header
	 */
	static void InitHeader(PacketHeader& header, MacAddr selfLladdr, uint16_t port);

	/**
	 * @brief Complete a previously initialized header
	 */
	static void PrepareHeader(PacketHeader& header, std::mt19937 rng, uint32_t ip, uint16_t port, const CandidateData& rawTxData, const Packet& payload);
} __attribute__ ((packed));

#endif
