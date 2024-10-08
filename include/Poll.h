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
	enum Event : uint16_t
	{
		In 	= 0x1,
		Out 	= 0x2,
	};
	
	virtual ~Poll() = default;

	/**
	 * Signaling functions.
	 */
	virtual void Signal() = 0;
	
	/**
	 * Add a file descriptor
	 * 
	 * @return Whether the file descriptor was added successfully
	 */
	virtual bool AddFd(int fd) = 0;
	
	/**
	 * Remove a file descriptor
	 * 
	 * @return Whether the file descriptor was removed successfully
	 */
	virtual bool RemoveFd(int fd) = 0;
	
	/**
	 * Clear all the file descriptors
	 */
	virtual void Clear() = 0;
	
	/**
	 * Wait until events occur or time is out
	 * 
	 * @param timeOutMs The time out, in milliseconds
	 * 
	 * @return The error code is a non-zero value in case of an error. Zero means no error.
	 */
	virtual int Wait(uint32_t timeOutMs) = 0;
	
	/**
	 * Iterate through all the file descriptors
	 * 
	 * @param func The function would be called for each file descriptor.
	 * 
	 * @return If the optional is set, it is regarded the intention is to quit the loop and the value is the exit code.
	 */
	virtual void ForEachFd(std::function<void(int)> func) = 0;
	
	/**
	 * Set the event mask. The mask is a value OR-ed by multiple Event types.
	 * 
	 * @return Whether the event mask was set successfully 
	 */
	virtual bool SetEventMask(int fd, uint16_t eventMask) = 0;
	
	/**
	 * Get the waited events of a file descriptor
	 * 
	 * @return The waited events and error code. The events is value OR-ed by multiple Event types. The error code is a non-zero
	 *         value in case of an error. Zero means no error.
	 */
	virtual std::pair<uint16_t, int> GetEvents(int fd) const = 0;
};

#endif
