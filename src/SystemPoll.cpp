#include "SystemPoll.h"
#include "config.h"
#include "tools.h"

#include <cassert>
#include <sys/poll.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

bool SystemPoll::AddFd(PollFd::Category category, int fd)
{
	if (fd == FD_INVALID) return false;
	
	pollfd syspfd;
	syspfd.fd = fd;
	
	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(syspfd.fd,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	
	if (fcntl(syspfd.fd,F_SETFL,fsflags) < 0)
		return false;
	
	pfds.emplace(PollFd{category, fd}, syspfd);
	
	tempfdsDirty = true;
	sysfdsDirty = true;
	
	return true;
}

bool SystemPoll::RemoveFd(PollFd::Category category, int fd)
{
	bool removed = pfds.erase({category, fd}) > 0;
	if (removed)
	{
		tempfdsDirty = true;
		sysfdsDirty = true;
	
		return true;
	}
	
	return false;
}

void SystemPoll::Clear()
{
	if (pfds.empty()) return;
	
	pfds.clear();
	tempfdsDirty = true;
	sysfdsDirty = true;
}


void SystemPoll::ForEachFd(PollFd::Category category, std::function<void(int)> func)
{
	if (tempfdsDirty)
	{
		tempfds.clear();
		for (auto& [fd, syspfd] : pfds)
		{
			tempfds.push_back(fd);
		}
		
		tempfdsDirty = false;
	}
	
	for (auto& fd : tempfds)
	{
		if (fd.category == category)
		{
			func(fd.fd);
		}
	}
}

bool SystemPoll::SetEventMask(PollFd::Category category, int fd, uint16_t eventMask)
{
	PollFd pfd = {category, fd};
	if (pfds.find(pfd) == pfds.end()) return false;
	
	auto& syspfd = pfds[pfd];
	syspfd.events = 0;
	
	if (eventMask & Event::In)
	{
		syspfd.events |= POLLIN;
	}

	if (eventMask & Event::Out)
	{
		syspfd.events |= POLLOUT;
	}
	
	syspfd.events |= POLLERR | POLLHUP;

	return true;
}

std::pair<uint16_t, int> SystemPoll::GetEvents(PollFd::Category category, int fd) const
{
	PollFd pfd = {category, fd};
	if (pfds.find(pfd) == pfds.end()) return std::make_pair<>(0, 0);
	
	uint events = 0;
	uint16_t revents = pfds.at(pfd).revents;
	
	if ((revents & POLLHUP) || (revents & POLLERR))
	{
		return std::make_pair<>(0, errno);
	}
	
	if (revents & POLLIN)
	{
		events |= Event::In;
	}
	
	if (revents & POLLOUT)
	{
		events |= Event::Out;
	}
	
	return std::make_pair<>(events, 0);
}

bool SystemPoll::Wait(uint32_t timeOutMs)
{
	if (sysfdsDirty)
	{
		syspfds.clear();
		indices.clear();
		
		for (auto& it : pfds)
		{
			syspfds.push_back(it.second);
			indices[syspfds.size() - 1] = it.first;
		}
		
		sysfdsDirty = false;
	}
	
	for (size_t i = 0; i < syspfds.size(); i++)
	{
		syspfds[i].events = pfds[indices[i]].events;
		syspfds[i].revents = 0;
	}
	
	// Wait for events
	if (poll(syspfds.data(), syspfds.size(),timeOutMs) < 0)
		return false;
	
	// Copy back
	for (size_t i = 0; i < syspfds.size(); i++)
	{
		assert(indices[i].fd == syspfds[i].fd);
		pfds[indices[i]] = syspfds[i];
	}

	return true;
}

