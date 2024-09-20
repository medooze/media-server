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

class Poll
{
public:
	enum class Category
	{
		IO,
		Signaling
	};
	
	enum Event
	{
		In 	= 0x1,
		Out 	= 0x2,
	};
	
	struct FileDescriptor
	{
		Category category;
		int fd;
		
		bool operator==(const FileDescriptor& other) const
		{
			return fd == other.fd && category == other.category;
		}
	};
	
	virtual ~Poll() = default;

	virtual bool AddFd(FileDescriptor fd) = 0;
	virtual bool RemoveFd(FileDescriptor fd) = 0;
	
	virtual void ForEach(std::function<void(FileDescriptor)> func) = 0;
	
	virtual bool SetEventMask(FileDescriptor fd, uint16_t eventMask) = 0;
	virtual uint16_t GetEvents(FileDescriptor fd) const = 0;
	
	virtual bool Wait(uint32_t timeOutMs) = 0;
	
	virtual std::optional<std::string> GetError(FileDescriptor fd) const = 0;
};

struct FileDescriptorHash {
    size_t operator()(const Poll::FileDescriptor& fd) const
    {
        return std::hash<size_t>()((uint64_t(static_cast<std::underlying_type<Poll::Category>::type>(fd.category)) << 32) + uint32_t(fd.fd));
    }
};

class SystemPoll : public Poll
{
public:
	
	bool AddFd(FileDescriptor fd) override;
	bool RemoveFd(FileDescriptor fd) override;

	void ForEach(std::function<void(FileDescriptor)> func) override;

	bool SetEventMask(FileDescriptor fd, uint16_t eventMask) override;
	uint16_t GetEvents(FileDescriptor fd) const override;
	
	bool Wait(uint32_t timeOutMs) override;
	
	std::optional<std::string> GetError(FileDescriptor fd) const;
	
private:
	std::unordered_map<FileDescriptor, pollfd, FileDescriptorHash> ufds;
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
	
	inline Poll* GetPoll() { return poll.get(); }
	
	bool Start(int fd)
	{ 
		return GetPoll()->AddFd({Poll::Category::IO, fd}) && Start();
	}
	
	virtual bool Start();
	virtual bool Stop();
	
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
	
	virtual std::optional<uint16_t> GetPollEventMask(Poll::FileDescriptor pfd) const { return std::nullopt; };
	virtual bool OnPollIn(Poll::FileDescriptor pfd);
	virtual bool OnPollOut(Poll::FileDescriptor pfd);
	virtual bool OnPollError(Poll::FileDescriptor pfd, const std::string& errorMsg);
	virtual void OnThreadExit() {}

private:

	std::thread	thread;
	int		pipe[2]		= {FD_INVALID, FD_INVALID};
	std::unique_ptr<Poll>	poll;
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

	
class CustomizedEventLoop : public EventLoop
{
public:
	CustomizedEventLoop()
	{
	}
	
	bool StartWithFunction(std::function<void(void)> loop)
	{
		this->loop = loop;
		
		return EventLoop::Start();
	}
	
	void Run(const std::chrono::milliseconds &duration = std::chrono::milliseconds::max()) override
	{
		if (loop)
		{
			(*loop)();
			
			loop.reset();
		}
		else
		{
			EventLoop::Run(duration);
		}
	}
	
private:
	std::optional<std::function<void(void)>> loop;
};


#endif /* EVENTLOOP_H */

