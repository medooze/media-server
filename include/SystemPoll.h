#ifndef SYSTEMPOLL_H
#define SYSTEMPOLL_H

#include "Poll.h"

#include <sys/poll.h>
#include <unordered_map>
#include <stdint.h>
#include <functional>
#include <optional>
#include <string>

/**
 * Concrete Poll implementation for Linux system poll mechanism.
 */
class SystemPoll : public Poll
{
public:
	bool AddFd(PollFd::Category category, int fd) override;
	bool RemoveFd(PollFd::Category category, int fd) override;
	void Clear() override;
	bool Wait(uint32_t timeOutMs) override;
	bool SetEventMask(PollFd::Category category, int fd, uint16_t eventMask) override;

	void ForEachFd(PollFd::Category category, std::function<void(int)> func) override;
	std::pair<uint16_t, int> GetEvents(PollFd::Category category, int fd) const override;
	
private:
	std::unordered_map<PollFd, pollfd> pfds;
	
	// Cache for constant time complexity
	bool tempfdsDirty = false;
	std::vector<PollFd> tempfds;
	
	// Cache for constant time complexity
	bool sysfdsDirty = false;
	std::vector<pollfd> syspfds;
	std::unordered_map<size_t, PollFd> indices;
};

#endif
