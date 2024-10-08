#include "SystemPoll.h"
#include "config.h"
#include "tools.h"

#include <cassert>
#include <sys/poll.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

SystemPoll::SystemPoll()
{
	if (!SystemPoll::AddFd(signalling.GetFd()))
	{
		throw std::runtime_error("Failed to add signaling fd to event poll\n");
	}
	
	if (!SystemPoll::SetEventMask(signalling.GetFd(), Poll::Event::In))
	{
		throw std::runtime_error("Failed to set event mask\n");
	}
}

void SystemPoll::Signal()
{
	signalling.Signal();
}

bool SystemPoll::AddFd(int fd)
{
	if (fd == FD_INVALID) return false;
	
	pollfd syspfd;
	syspfd.fd = fd;
	
	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(syspfd.fd,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	
	if (fcntl(syspfd.fd,F_SETFL,fsflags) < 0)
		return false;
	
	pfds.emplace(fd, syspfd);
	
	tempfdsDirty = true;
	sysfdsDirty = true;
	
	return true;
}

bool SystemPoll::RemoveFd(int fd)
{
	bool removed = pfds.erase(fd) > 0;
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
	
	// Clear except the signalling fd
	for (auto it = pfds.begin(); it != pfds.end();)
	{
		if (it->first == signalling.GetFd())
		{
			++it;
		}
		else
		{
			it = pfds.erase(it);
		}
	}
	
	tempfdsDirty = true;
	sysfdsDirty = true;
}


void SystemPoll::ForEachFd(std::function<void(int)> func)
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
		if (fd != signalling.GetFd())
		{
			func(fd);
		}
	}
}

bool SystemPoll::SetEventMask(int fd, uint16_t eventMask)
{
	if (pfds.find(fd) == pfds.end()) return false;
	
	auto& syspfd = pfds[fd];
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

std::pair<uint16_t, int> SystemPoll::GetEvents(int fd) const
{
	if (pfds.find(fd) == pfds.end()) return std::make_pair<>(0, 0);
	
	uint events = 0;
	uint16_t revents = pfds.at(fd).revents;
	
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

int SystemPoll::Wait(uint32_t timeOutMs)
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
		return errno;
	
	// Copy back
	for (size_t i = 0; i < syspfds.size(); i++)
	{
		if (syspfds[i].fd == signalling.GetFd())
		{
			auto revents = syspfds[i].revents;
			if ((revents & POLLHUP) || (revents & POLLERR))
			{
				signalling.ClearSignal();
				return errno;
			}
			else if (revents & POLLIN)
			{
				signalling.ClearSignal();
			}
		}
		
		assert(indices[i] == syspfds[i].fd);
		pfds[indices[i]] = syspfds[i];
	}

	return 0;
}

