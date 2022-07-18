#ifndef _RAWTXHELPER_H_
#define _RAWTXHELPER_H_

#include <string>
#include <array>
#include <random>
#include "Packet.h"
#include "PacketHeader.h"

/**
 * @brief Simple helper to send UDP messages as raw Ethernet/IP/UDP
 * frames in 'typical' network configurations.
 *
 * Based on AF_PACKET (Linux-only), doesn't support PACKET_MMAP yet.
 */
class RawTxHelper
{
public:
	using MacAddr = std::array<uint8_t, 6>;
	RawTxHelper(
		// TX options
		int32_t ifindex, unsigned int sndbuf, bool skipQdisc,
		// frame header parameters
		uint32_t selfAddr, uint32_t prefixlen, MacAddr selfLladdr, uint32_t gwAddr, MacAddr gwLladdr, uint16_t port);

	RawTxHelper(RawTxHelper&& other) noexcept;
	RawTxHelper& operator=(RawTxHelper&& other) noexcept;
	RawTxHelper(const RawTxHelper& other) = delete;
	RawTxHelper& operator=(const RawTxHelper& other) = delete;
	~RawTxHelper();

	/**
	 * try to send a frame using raw TX. if false is returned,
	 * the packet has to be sent through normal means
	 */
	bool TrySend(uint32_t ip, uint16_t port, Packet&& payload);

protected:
	// AF_PACKET socket
	int fd = -1;
	// used to check for link-local traffic
	uint32_t selfAddr;
	uint32_t prefixlen;
	// used to build frame header
	PacketHeader header;
	// used to generate IP identification
	std::mt19937 rng;
};

#endif
