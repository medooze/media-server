#ifndef POLL_H
#define POLL_H

#include <stdint.h>
#include <stddef.h>
#include <optional>
#include <string>
#include <functional>

/**
 * A Poll object is used to allow EvenLoop to implement non-blocking logic. It can wait on multiple file descriptors
 * and listen on the events, which can be filtered by providing the event mask.
 * 
 * File descriptors are categorized to two types: IO and signaling. The IO category is for transferring data, such as
 * network connection. The signaling category is for breaking the waiting for processing tasks and triggers. The concrete
 * Poll class can implement different logic for different categories.
 * 
 * This class doesn't manage the file descriptor creation. It allows to add/remove file descriptors and set event mask
 * on them. The Wait() function would wait on all added file descriptors and block the current thread. In case of any 
 * event occurred on the file descriptors, the Wait() function would exit and all the events/errors can be queried by 
 * GetEvents()/GetError().
 */
class Poll
{
public:
	
	/**
	 * Event types.
	 */
	enum Event
	{
		In 	= 0x1,
		Out 	= 0x2,
	};
	
	/**
	 * A wrapper for file descriptor to include categories for polling.
	 */
	struct PollFd
	{
		enum class Category
		{
			IO,
			Signaling
		};
		
		bool operator==(const PollFd& other) const
		{
			return fd == other.fd && category == other.category;
		}
		
		Category category;
		int fd;
	};
	
	virtual ~Poll() = default;

	/**
	 * Add a file descriptor
	 * 
	 * @return Whether the file descriptor was added successfully
	 */
	virtual bool AddFd(PollFd pfd) = 0;
	
	/**
	 * Remove a file descriptor
	 * 
	 * @return Whether the file descriptor was removed successfully
	 */
	virtual bool RemoveFd(PollFd pfd) = 0;
	
	/**
	 * Clear all the file descriptors
	 */
	virtual void Clear() = 0;
	
	/**
	 * Wait until events occur or time is out
	 * 
	 * @param timeOutMs The time out, in milliseconds
	 * 
	 * @return Whether the polling is failed.
	 */
	virtual bool Wait(uint32_t timeOutMs) = 0;
	
	/**
	 * Iterate through all the file descriptors
	 * 
	 * @param func The function would be called for each file descriptor.
	 */
	virtual void ForEachFd(std::function<void(PollFd)> func) = 0;
	
	/**
	 * Set the event mask. The mask is a value OR-ed by multiple Event types.
	 * 
	 * @return Whether the event mask was set successfully 
	 */
	virtual bool SetEventMask(PollFd pfd, uint16_t eventMask) = 0;
	
	/**
	 * Get the waited events of a file descriptor
	 * 
	 * @return The waited events. It is value OR-ed by multiple Event types.
	 */
	virtual uint16_t GetEvents(PollFd pfd) const = 0;
	
	/**
	 * Get error message of a file descriptor
	 * 
	 * @return A optional containing the error message in case of error, std::nullopt otherwise.
	 */
	virtual std::optional<std::string> GetError(PollFd pfd) const = 0;
};

/**
 * Hash function for PollFd
 */
template<>
struct std::hash<Poll::PollFd>
{
	size_t operator()(const Poll::PollFd& fd) const
	{
		// While this hash function works when size of int is not 4, but it might be better way to
		// calculate in that case. Assert here for reminder.
		static_assert(sizeof(int) == 4);
		return (uint64_t(static_cast<std::underlying_type<Poll::PollFd::Category>::type>(fd.category)) << 32) + uint32_t(fd.fd);
	}
};


#endif
