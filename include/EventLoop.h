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
	EventLoop();
	virtual ~EventLoop();
	
	bool Start(std::function<void(void)> loop);
	bool Start(int fd = FD_INVALID);
	bool Stop();
	
	virtual const std::chrono::milliseconds GetNow() const override { return now; }
	virtual Timer::shared CreateTimer(const std::function<void(std::chrono::milliseconds)>& callback) override;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& timeout) override;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& timeout) override;
	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func) override;
	virtual void Async(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback) override;
	virtual std::future<void> Future(const std::function<void(std::chrono::milliseconds)>& func) override;
	
	void Run(const std::chrono::milliseconds &duration = std::chrono::milliseconds::max());
	
	bool SetAffinity(int cpu);
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
	const auto GetPipe() const
	{
		return pipe;
	}

	const std::chrono::milliseconds Now();
	
	virtual short GetPollEvents() const { return POLLIN | POLLERR | POLLHUP; };
	virtual void OnPollIn(int fd) {};
	virtual void OnPollOut(int fd) {};
	
private:

	std::thread	thread;
	int		fd		= 0;
	int		pipe[2]		= {FD_INVALID, FD_INVALID};
	pollfd		ufds[2]		= {};
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

