#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <thread>
#include <chrono>
#include <optional>
#include "concurrentqueue.h"
#include "TimeService.h"
#include "FileDescriptor.h"
#include "SystemPoll.h"

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
	EventLoop(std::unique_ptr<Poll> poll = std::make_unique<SystemPoll>());
	virtual ~EventLoop();
	
	bool StartWithLoop(std::function<void(void)> loop);
	
	bool StartWithFd(int fd);
	
	bool Start();
	bool Stop();
	
	virtual const std::chrono::milliseconds GetNow() const override { return now; }
	virtual Timer::shared CreateTimerUnsafe(const std::function<void(std::chrono::milliseconds)>& callback) override;
	virtual Timer::shared CreateTimerUnsafe(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& timeout) override;
	virtual Timer::shared CreateTimerUnsafe(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& timeout) override;
	virtual void AsyncUnsafe(const std::function<void(std::chrono::milliseconds)>& func) override;
	virtual void AsyncUnsafe(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback) override;
	virtual std::future<void> FutureUnsafe(const std::function<void(std::chrono::milliseconds)>& func) override;
	
	virtual void Run(const std::chrono::milliseconds &duration = std::chrono::milliseconds::max());
	
	virtual bool SetAffinity(int cpu);
	
	bool SetThreadName(const std::string& name);
	bool SetPriority(int priority);
	bool IsRunning() const { return running; }

protected:
	/**
	 * Predefined exit codes. They are all negative values. The user defined exit code must be
	 * non-negative.
	 */
	enum class PredefinedExitCode : int
	{
		WaitError = -1,
		SignalingError = -2
	};

	void Signal();
	inline void AssertThread() const { assert(std::this_thread::get_id()==thread.get_id()); }
	void CancelTimer(TimerImpl::shared timer);
	
	void ProcessTasks(const std::chrono::milliseconds& now);
	void ProcessTriggers(const std::chrono::milliseconds& now);
	int  GetNextTimeout(int defaultTimeout, const std::chrono::milliseconds& until = std::chrono::milliseconds::max()) const;

	const std::chrono::milliseconds Now();
	
	// Functions to add/remove/iterate file descriptors
	
	bool AddFd(int fd, std::optional<uint16_t> eventMask = std::nullopt);
	bool RemoveFd(int fd);
	void ForEachFd(const std::function<void(int)>& func);
	
	/**
	 * Set stopping. This will trigger the current loop to exit.
	 */
	inline void SetStopping(int code)
	{
		exitCode = code;
	}
	
	/**
	 * Get updated event mask for a file descriptor. 
	 * 
	 * Note if the return optional doesn't have value, the current event mask wouldn't be changed.
	 */
	virtual std::optional<uint16_t> GetPollEventMask(int fd) const { return std::nullopt; };
	
	/**
	 * Callback to be called when it is ready to read from the file descriptor. 
	 */
	virtual void OnPollIn(int fd) {};
	
	/**
	 * Callback to be called when it is ready to write to the file descriptor.
	 */
	virtual void OnPollOut(int fd) {};
	
	/**
	 * Callback to be called when error occured on the file descriptor.
	 */
	virtual void OnPollError(int fd, int errorCode) {};
	
	/**
	 * Called when the Run() function was entered.
	 */
	virtual void OnLoopEnter() {};
	
	/**
	 * Called when the Run() function was exited.
	 */
	virtual void OnLoopExit(int exitCode) {};

private:

	std::thread			thread;
	std::unique_ptr<Poll>		poll;
	volatile bool	running		= false;
	std::chrono::milliseconds now	= 0ms;
	moodycamel::ConcurrentQueue<
		std::pair<
			std::function<void(std::chrono::milliseconds)>,
			std::optional<std::function<void(std::chrono::milliseconds)>>
		>
	>  tasks;
	std::multimap<std::chrono::milliseconds,TimerImpl::shared> timers;
	
	std::optional<int> exitCode;
};

#endif /* EVENTLOOP_H */

