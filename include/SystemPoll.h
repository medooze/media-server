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
	bool AddFd(PollFd pfd) override;
	bool RemoveFd(PollFd pfd) override;
	void Clear() override;
	bool Wait(uint32_t timeOutMs) override;

	void ForEachFd(std::function<void(PollFd)> func) override;
	bool SetEventMask(PollFd pfd, uint16_t eventMask) override;
	uint16_t GetEvents(PollFd pfd) const override;
	std::optional<std::string> GetError(PollFd pfd) const override;
	
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
