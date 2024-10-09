#ifndef SYSTEMPOLL_H
#define SYSTEMPOLL_H

#include "Poll.h"
#include "PollSignalling.h"

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
	SystemPoll();

	void Signal() override;
	
	bool AddFd(int fd) override;
	bool RemoveFd(int fd) override;
	void Clear() override;
	int Wait(uint32_t timeOutMs) override;
	bool SetEventMask(int fd, uint16_t eventMask) override;

	void ForEachFd(std::function<void(int)> func) override;
	std::pair<uint16_t, int> GetEvents(int fd) const override;
	
private:
	std::unordered_map<int, pollfd> pfds;
	
	// Cache for constant time complexity
	bool tempfdsDirty = true;
	std::vector<int> tempfds;
	
	// Cache for constant time complexity
	bool sysfdsDirty = true;
	std::vector<pollfd> syspfds;
	std::unordered_map<size_t, int> indices;
	
	PollSignalling signalling;
};

#endif
