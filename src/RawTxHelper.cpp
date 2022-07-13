#include "tracing.h"
#include "RawTxHelper.h"
#include <stdexcept>
#include <system_error>

#ifndef __linux__
RawTxHelper::RawTxHelper(int32_t ifindex, unsigned int sndbuf, bool skipQdisc, uint32_t self_addr, uint32_t prefixlen, MacAddr self_lladdr, uint32_t gw_addr, MacAddr gw_lladdr, uint16_t port)
{
	throw std::runtime_error("raw TX is only supported in Linux");
}
RawTxHelper::RawTxHelper(RawTxHelper&& other) noexcept {}
RawTxHelper& RawTxHelper::operator=(RawTxHelper&& other) noexcept { return *this; }
RawTxHelper::~RawTxHelper() {}
bool RawTxHelper::TrySend(uint32_t ip, uint16_t port, Packet&& packet) { return false; }
#else

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <fcntl.h>

struct IpHeader {
	uint8_t ihl : 4;
	uint8_t version : 4;
	uint8_t tos;
	uint16_t total_len;
	uint16_t identification;
	uint8_t flags : 3;
	uint16_t frag_offset : 13;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t hdr_checksum;
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

struct FrameTemplate {
	ether_header eth;
	union {
		IpHeader ip;
		// for checksum calculation
		uint16_t ip_hws [sizeof(IpHeader) / 2];
	};
	union {
		UdpHeader udp;
		// for checksum calculation
		uint16_t udp_hws [sizeof(UdpHeader) / 2];
	};
} __attribute__ ((packed));

RawTxHelper::RawTxHelper(RawTxHelper&& other) noexcept:
	self_addr(other.self_addr), prefixlen(other.prefixlen),
	frame_template(std::move(other.frame_template)),
	rng(std::move(other.rng))
{
	std::swap(fd, other.fd);
}

RawTxHelper& RawTxHelper::operator=(RawTxHelper&& other) noexcept
{
	self_addr = other.self_addr, prefixlen = other.prefixlen;
	frame_template = std::move(other.frame_template);
	rng = std::move(other.rng);
	std::swap(fd, other.fd);
	return *this;
}

RawTxHelper::RawTxHelper(int32_t ifindex, unsigned int sndbuf, bool skipQdisc, uint32_t self_addr, uint32_t prefixlen, MacAddr self_lladdr, uint32_t gw_addr, MacAddr gw_lladdr, uint16_t port):
	self_addr(self_addr), prefixlen(prefixlen), rng(std::random_device()())
{
	// prepare frame template

	FrameTemplate frame;
	memcpy(frame.eth.ether_dhost, gw_lladdr.data(), 6);
	memcpy(frame.eth.ether_shost, self_lladdr.data(), 6);
	frame.eth.ether_type = htons(ETHERTYPE_IP);
	frame.ip.ihl = sizeof(frame.ip) / 4;
	frame.ip.version = 4;
	frame.ip.tos = 0x2E;
	frame.ip.flags = 0;
	frame.ip.frag_offset = 0;
	frame.ip.ttl = 64;
	frame.ip.protocol = IPPROTO_UDP;
	frame.ip.src = htonl(self_addr);
	frame.udp.src = htons(port);
	frame_template = std::string((const char*)&frame, sizeof(frame));

	// set up AF_PACKET socket

	// protocol=0 means no RX
	if ((fd = socket(PF_PACKET, SOCK_RAW, 0)) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed creating AF_PACKET socket");

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

	struct sockaddr_ll bind_addr;
	bind_addr.sll_family = AF_PACKET;
	bind_addr.sll_ifindex = ifindex;
	bind_addr.sll_protocol = 0;
	if (bind(fd, (sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
		close(fd);
		throw std::system_error(std::error_code(errno, std::system_category()), "failed binding AF_PACKET socket");
	}

	if (sndbuf > 0 && setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
		close(fd);
		throw std::system_error(std::error_code(errno, std::system_category()), "failed setting send queue size");
	}

	int skipQdiscInt = 1;
	if (skipQdisc && setsockopt(fd, SOL_PACKET, PACKET_QDISC_BYPASS, &skipQdiscInt, sizeof(skipQdiscInt)) < 0) {
		close(fd);
		throw std::system_error(std::error_code(errno, std::system_category()), "failed setting QDISC_BYPASS");
	}
}

RawTxHelper::~RawTxHelper()
{
	if (fd >= 0 && close(fd) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed closing AF_PACKET socket");
}

struct UdpIpv4PseudoHeader {
	uint32_t src;
	uint32_t dst;
	uint8_t zero;
	uint8_t protocol;
	uint16_t udp_length;
} __attribute__ ((packed));

bool RawTxHelper::TrySend(uint32_t ip, uint16_t port, Packet&& packet)
{
	if (fd < 0)
		return false;

	// for safety, if destination ip is local, send in the usual way
	uint32_t mask = (~0U) << (32 - prefixlen);
	if (!((ip ^ self_addr) & mask))
		return false;

	// set missing fields
	FrameTemplate& frame = *(FrameTemplate*)&frame_template[0];
	frame.ip.dst = htonl(ip);
	frame.udp.dst = htons(port);
	frame.udp.length = htons(sizeof(frame.udp) + packet.GetSize());
	frame.ip.total_len = htons((sizeof(frame) - sizeof(frame.eth)) + packet.GetSize());
	std::uniform_int_distribution<uint16_t> u16_distr;
	frame.ip.identification = u16_distr(rng);

	uint32_t checksum;

	// calculate IP header checksum
	frame.ip.hdr_checksum = 0;
	checksum = 0;
	for (size_t i = 0; i < (sizeof(frame.ip_hws) / sizeof(*frame.ip_hws)); i++)
		checksum += frame.ip_hws[i];
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	frame.ip.hdr_checksum = ~checksum;

	// calculate UDP checksum
	union {
		UdpIpv4PseudoHeader hdr;
		uint16_t words [sizeof(UdpIpv4PseudoHeader) / 2];
	} udp_phdr;
	udp_phdr.hdr = { frame.ip.src, frame.ip.dst, 0, frame.ip.protocol, frame.udp.length };

	frame.udp.checksum = 0;
	checksum = 0;
	for (size_t i = 0; i < (sizeof(udp_phdr.words) / sizeof(*udp_phdr.words)); i++)
		checksum += udp_phdr.words[i];
	for (size_t i = 0; i < (sizeof(frame.udp_hws) / sizeof(*frame.udp_hws)); i++)
		checksum += frame.udp_hws[i];
	uint16_t* data_words = (uint16_t*) packet.GetData();
	size_t data_nwords = packet.GetSize() / 2;
	for (size_t i = 0; i < data_nwords; i++)
		checksum += data_words[i];
	if (packet.GetSize() % 2 != 0)
		checksum += uint16_t(packet.GetData()[packet.GetSize() - 1]);
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	frame.udp.checksum = ~checksum;
	if (!frame.udp.checksum) frame.udp.checksum = 0xFFFF;

	// send frame
	struct iovec iov [2] = { { &frame, sizeof(frame) }, { packet.GetData(), packet.GetSize() } };
	struct msghdr msg = { nullptr, 0, iov, sizeof(iov)/sizeof(*iov), nullptr, 0, 0 };

	TRACE_EVENT("eventloop", "RawTxHelper::TrySend::sendmsg");
	if (sendmsg(fd, &msg, 0) < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			TRACE_EVENT("eventloop", "RawTxHelper::TrySend::Dropped");
		} else {
			throw std::system_error(std::error_code(errno, std::system_category()), "failed sending frame");
		}
	}
	return true;
}

#endif
