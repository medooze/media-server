#include "tracing.h"
#include "RawTxHelper.h"
#include "log.h"
#include <stdexcept>
#include <system_error>

#ifndef __linux__
RawTxHelper::RawTxHelper(int32_t ifindex, unsigned int sndbuf, bool skipQdisc, uint32_t selfAddr, uint32_t prefixlen, MacAddr selfLladdr, uint32_t gwAddr, MacAddr gwLladdr, uint16_t port)
{
	throw std::runtime_error("raw TX is only supported in Linux");
}
bool RawTxHelper::TrySend(uint32_t ip, uint16_t port, Packet&& payload) { return false; }
#else

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <fcntl.h>

RawTxHelper::RawTxHelper(int32_t ifindex, unsigned int sndbuf, bool skipQdisc, uint32_t selfAddr, uint32_t prefixlen, MacAddr selfLladdr, uint32_t gwAddr, MacAddr gwLladdr, uint16_t port):
	selfAddr(selfAddr), prefixlen(prefixlen), rng(std::random_device()())
{
	// prepare frame template

	PacketHeader::InitHeader(header, selfAddr, selfLladdr, gwAddr, gwLladdr, port);

	// set up AF_PACKET socket

	// protocol=0 means no RX
	if ((fd = FileDescriptor(socket(PF_PACKET, SOCK_RAW, 0))) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed creating AF_PACKET socket");

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

	struct sockaddr_ll bindAddr;
	bindAddr.sll_family = AF_PACKET;
	bindAddr.sll_ifindex = ifindex;
	bindAddr.sll_protocol = 0;
	if (bind(fd, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed binding AF_PACKET socket");

	if (sndbuf > 0 && setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed setting send queue size");

	int skipQdiscInt = 1;
	if (skipQdisc && setsockopt(fd, SOL_PACKET, PACKET_QDISC_BYPASS, &skipQdiscInt, sizeof(skipQdiscInt)) < 0)
		throw std::system_error(std::error_code(errno, std::system_category()), "failed setting QDISC_BYPASS");
}

bool RawTxHelper::TrySend(uint32_t ip, uint16_t port, Packet&& payload)
{
	if (fd < 0)
		return false;

	// for safety, if destination ip is local, send in the usual way
	uint32_t mask = (~0U) << (32 - prefixlen);
	if (!((ip ^ selfAddr) & mask))
		return false;

	// set missing fields
	PacketHeader::PrepareHeader(header, rng, ip, port, payload);

	// send frame
	struct iovec iov [2] = { { &header, sizeof(header) }, { payload.GetData(), payload.GetSize() } };
	struct msghdr msg = { nullptr, 0, iov, sizeof(iov)/sizeof(*iov), nullptr, 0, 0 };

	TRACE_EVENT("eventloop", "RawTxHelper::TrySend::sendmsg");
	if (sendmsg(fd, &msg, 0) < 0) {
		// AF_PACKET seems to return ENOSPC instead of blocking
		if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ENOSPC) {
			TRACE_EVENT("eventloop", "RawTxHelper::TrySend::Dropped");
		} else {
			Warning("-RawTxHelper::TrySend() | failed sending frame: %s\n", strerror(errno));
		}
	}
	return true;
}

#endif
