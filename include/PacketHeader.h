#ifndef _PACKETHEADER_H_
#define _PACKETHEADER_H_

#include <cstdint>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include "MacAddress.h"
#include "Packet.h"

/**
 * @brief Provides structs and logic to build Ethernet packet headers
 */
struct PacketHeader {
	struct FlowRoutingInfo {
		uint32_t selfAddr;
		MacAddress dstLladdr;
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
	void CalculateIpChecksum();

	/**
	 * @brief Calculate and (re)set the UDP checksum
	 */
	void CalculateUdpChecksum(const Packet& payload);

	/**
	 * @brief Initialize the frame header
	 */
	static PacketHeader Create(const MacAddress& selfLladdr, uint16_t port);

	/**
	 * @brief Complete a previously initialized header
	 */
	static void PrepareHeader(PacketHeader& header, uint32_t ip, uint16_t port, const FlowRoutingInfo& rawTxData, const Packet& payload);
} __attribute__ ((packed));

#endif
