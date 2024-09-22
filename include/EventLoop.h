#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <thread>
#include <chrono>
#include <optional>
#include <poll.h>
#include "concurrentqueue.h"
#include "TimeService.h"
#include "FileDescriptor.h"

using namespace std::chrono_literals;

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
	 * A wrapper for file descriptor to include categories.
	 */
	struct FileDescriptor
	{
		enum class Category
		{
			IO,
			Signaling
		};
		
		bool operator==(const FileDescriptor& other) const
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
	virtual bool AddFd(FileDescriptor fd) = 0;
	
	/**
	 * Remove a file descriptor
	 * 
	 * @return Whether the file descriptor was removed successfully
	 */
	virtual bool RemoveFd(FileDescriptor fd) = 0;
	
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
	virtual void ForEachFd(std::function<void(FileDescriptor)> func) = 0;
	
	/**
	 * Set the event mask. The mask is a value OR-ed by multiple Event types.
	 * 
	 * @return Whether the event mask was set successfully 
	 */
	virtual bool SetEventMask(FileDescriptor fd, uint16_t eventMask) = 0;
	
	/**
	 * Get the waited events of a file descriptor
	 * 
	 * @return The waited events. It is value OR-ed by multiple Event types.
	 */
	virtual uint16_t GetEvents(FileDescriptor fd) const = 0;
	
	/**
	 * Get error message of a file descriptor
	 * 
	 * @return A optional containing the error message in case of error, std::nullopt otherwise.
	 */
	virtual std::optional<std::string> GetError(FileDescriptor fd) const = 0;
};

/**
 * Hash function for constant access to association array.
 */
struct FileDescriptorHash
{
	size_t operator()(const Poll::FileDescriptor& fd) const
	{
		return std::hash<size_t>()((uint64_t(static_cast<std::underlying_type<Poll::FileDescriptor::Category>::type>(fd.category)) << 32) + uint32_t(fd.fd));
	}
};

/**
 * Concrete Poll implementation for Linux system poll mechanism.
 */
class SystemPoll : public Poll
{
public:
	bool AddFd(FileDescriptor fd) override;
	bool RemoveFd(FileDescriptor fd) override;
	void Clear() override;
	bool Wait(uint32_t timeOutMs) override;

	void ForEachFd(std::function<void(FileDescriptor)> func) override;
	bool SetEventMask(FileDescriptor fd, uint16_t eventMask) override;
	uint16_t GetEvents(FileDescriptor fd) const override;
	std::optional<std::string> GetError(FileDescriptor fd) const;
	
private:
	std::unordered_map<FileDescriptor, pollfd, FileDescriptorHash> ufds;
	
	// Cache for constant access time
	bool tempfdsDirty = false;
	std::vector<FileDescriptor> tempfds;
	
	// Cache for constant access time
	bool sysfdsDirty = false;
	std::vector<pollfd> sysfds;
	std::unordered_map<size_t, FileDescriptor> indices;
};

class EventLoop : public TimeService
{
public:
	static bool SetAffinity(std::thread::native_handle_type thread, int cpu);
	static bool SetThreadName(std::thread::native_handle_type thread, const std::string& name);
private:
	class TimerImpl : 
		public Timer, 
		public std::enable_shared_from_this<TimerImpl>
	{
	public:
		using shared = std::shared_ptr<TimerImpl>;
		
		TimerImpl(EventLoop& loop, const std::chrono::milliseconds& repeat, std::function<void(std::chrono::milliseconds) > callback) :
			loop(loop),
			next(0),
			repeat(repeat),
			callback(callback)
		{
		}
			
		virtual ~TimerImpl() 
		{
		}
		
		TimerImpl(const TimerImpl&) = delete;
		virtual void Cancel() override;
		virtual void Again(const std::chrono::milliseconds& ms) override;
		virtual void Repeat(const std::chrono::milliseconds& repeat) override;
		virtual void Reschedule(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat) override;
		virtual bool IsScheduled()			const override { return next.count();	}
		virtual std::chrono::milliseconds GetNextTick()	const override { return next;		}
		virtual std::chrono::milliseconds GetRepeat()	const override { return repeat;		}
		
		EventLoop&		  loop;
		std::chrono::milliseconds next;
		std::chrono::milliseconds repeat;
		std::function<void(std::chrono::milliseconds)> callback;
	};
	
public:
	EventLoop(std::unique_ptr<Poll> poll = std::make_unique<SystemPoll>());
	virtual ~EventLoop();
	
	bool Start(std::function<void(void)> loop);
	
	bool StartWithFd(int fd);
	
	bool Start();
	bool Stop();
	
	virtual const std::chrono::milliseconds GetNow() const override { return now; }
	virtual Timer::shared CreateTimer(const std::function<void(std::chrono::milliseconds)>& callback) override;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& timeout) override;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& timeout) override;
	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func) override;
	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback) override;
	virtual std::future<void> Future(const std::function<void(std::chrono::milliseconds)>& func) override;
	
	virtual void Run(const std::chrono::milliseconds &duration = std::chrono::milliseconds::max());
	
	virtual bool SetAffinity(int cpu);
	
	bool SetThreadName(const std::string& name);
	bool SetPriority(int priority);
	bool IsRunning() const { return running; }

protected:
	void Signal();
	void ClearSignal();
	inline void AssertThread() const { assert(std::this_thread::get_id()==thread.get_id()); }
	void CancelTimer(TimerImpl::shared timer);
	
	void ProcessTasks(const std::chrono::milliseconds& now);
	void ProcessTriggers(const std::chrono::milliseconds& now);
	int  GetNextTimeout(int defaultTimeout, const std::chrono::milliseconds& until = std::chrono::milliseconds::max()) const;

	const std::chrono::milliseconds Now();
	
	// Functions to add/remove/iterate file descriptors
	
	inline bool AddFd(int fd, std::optional<uint16_t> eventMask = std::nullopt)
	{
		Poll::FileDescriptor pfd = {Poll::FileDescriptor::Category::IO, fd};
		if (!poll->AddFd(pfd)) return false;
		
		if (eventMask.has_value())
		{
			return poll->SetEventMask(pfd, *eventMask);
		}
		
		return true;
	}
	
	inline bool RemoveFd(int fd)
	{
		return poll->RemoveFd({Poll::FileDescriptor::Category::IO, fd});
	}
	
	inline void ForEachFd(const std::function<void(int)>& func)
	{
		poll->ForEachFd([&func](Poll::FileDescriptor fd) {
			if (fd.category == Poll::FileDescriptor::Category::IO) func(fd.fd);
		});
	}
	
	/**
	 * Get updated event mask for a file descriptor. Note if the return optional doesn't have value, the
	 * current event mask wouldn't be changed.
	 */
	virtual std::optional<uint16_t> GetPollEventMask(int pfd) const { return std::nullopt; };
	
	/**
	 * Callback to be called when it is ready to read from the file descriptor. If exception throws, the loop
	 * will exit.
	 */
	virtual void OnPollIn(int pfd) { };
	
	/**
	 * Callback to be called when it is ready to read from the file descriptor. If exception throws, the loop
	 * will exit.
	 */
	virtual void OnPollOut(int pfd) { };
	
	/**
	 * Callback to be called when it is ready to read from the file descriptor.If exception throws, the loop
	 * will exit.
	 */
	virtual void OnPollError(int pfd, const std::string& errorMsg) { };
	
	/**
	 * Called when the Run() function was exited.
	 */
	virtual void OnLoopExit() { };

private:

	std::thread			thread;
	
	//@todo It looks we only need one pipe for signaling
	int		pipe[2]		= {FD_INVALID, FD_INVALID};
	std::unique_ptr<Poll>		poll;
	std::atomic_flag signaled	= ATOMIC_FLAG_INIT;
	volatile bool	running		= false;
	std::chrono::milliseconds now	= 0ms;
	moodycamel::ConcurrentQueue<
		std::pair<
			std::function<void(std::chrono::milliseconds)>,
			std::optional<std::function<void(std::chrono::milliseconds)>>
		>
	>  tasks;
	std::multimap<std::chrono::milliseconds,TimerImpl::shared> timers;
};

#endif /* EVENTLOOP_H */

