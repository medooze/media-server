#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <thread>
#include <functional>
#include <chrono>
#include <poll.h>
#include <cassert>
#include "config.h"
#include "concurrentqueue.h"
#include "Packet.h"
#include "TimeService.h"

using namespace std::chrono_literals;

class EventLoop : public TimeService
{
public:
	class Listener
	{
	public:
		virtual ~Listener() = default;
		virtual void OnRead(const int fd, const uint8_t* data, const size_t size, const uint32_t ipAddr, const uint16_t port) = 0;
	};
	enum State
	{
		Normal,
		Lagging,
		Overflown
	};
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
		virtual bool IsScheduled()			const override { return next.count();	}
		virtual std::chrono::milliseconds GetNextTick()	const override { return next;		}
		virtual std::chrono::milliseconds GetRepeat()	const override { return repeat;		}
		
		EventLoop&		  loop;
		std::chrono::milliseconds next;
		std::chrono::milliseconds repeat;
		std::function<void(std::chrono::milliseconds)> callback;
	};
public:
	EventLoop(Listener* listener = nullptr);
	virtual ~EventLoop();
	
	bool Start(std::function<void(void)> loop);
	bool Start(int fd = FD_INVALID);
	bool Stop();
	
	virtual const std::chrono::milliseconds GetNow() const override { return now; }
	virtual Timer::shared CreateTimer(std::function<void(std::chrono::milliseconds)> callback) override;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, std::function<void(std::chrono::milliseconds)> timeout) override;
	virtual Timer::shared CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, std::function<void(std::chrono::milliseconds)> timeout) override;
	virtual std::future<void> Async(std::function<void(std::chrono::milliseconds)> func) override;
	
	void Send(const uint32_t ipAddr, const uint16_t port, Packet&& packet);
	void Run(const std::chrono::milliseconds &duration = std::chrono::milliseconds::max());
	
	bool SetAffinity(int cpu);
	
	bool IsRunning() const { return running; }
	
protected:
	void Signal();
	inline void AssertThread() const { assert(std::this_thread::get_id()==thread.get_id()); }
	void CancelTimer(TimerImpl::shared timer);
	
	const std::chrono::milliseconds Now();
private:
	struct SendBuffer
	{
		//NO copyable
		SendBuffer() = default; 
		SendBuffer(const SendBuffer&) = delete; 
		SendBuffer(SendBuffer&&) = default; 
		SendBuffer& operator=(SendBuffer const&) = delete;
		SendBuffer& operator=(SendBuffer&&) = default;
		
		uint32_t ipAddr;
		uint16_t port;
		Packet   packet;
	};
	static const size_t MaxSendingQueueSize;
	static const size_t MaxMultipleSendingMessages;
private:
	std::thread	thread;
	State		state		= State::Normal;
	Listener*	listener	= nullptr;
	int		fd		= 0;
	int		pipe[2]		= {FD_INVALID, FD_INVALID};
	pollfd		ufds[2]		= {};
	volatile bool	signaled	= false;
	volatile bool	running		= false;
	std::chrono::milliseconds now	= 0ms;
	moodycamel::ConcurrentQueue<SendBuffer>	sending;
	moodycamel::ConcurrentQueue<std::pair<std::promise<void>,std::function<void(std::chrono::milliseconds)>>>  tasks;
	std::multimap<std::chrono::milliseconds,TimerImpl::shared> timers;
	
};

#endif /* EVENTLOOP_H */

