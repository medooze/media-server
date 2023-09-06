#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <thread>
#include <functional>
#include <chrono>
#include <optional>
#include <poll.h>
#include <cassert>
#include <optional>
#include "config.h"
#include "concurrentqueue.h"
#include "Packet.h"
#include "ObjectPool.h"
#include "TimeService.h"
#include "FileDescriptor.h"
#include "PacketHeader.h"

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
	
	struct RawTx
	{
		FileDescriptor fd;
		PacketHeader header;
		PacketHeader::FlowRoutingInfo defaultRoute;

		RawTx(const FileDescriptor& fd, const PacketHeader& header, const PacketHeader::FlowRoutingInfo& defaultRoute)	:
			fd(fd),
			header(header),
			defaultRoute(defaultRoute)
		{
		}
	};
public:
	EventLoop(Listener* listener = nullptr, uint32_t packetPoolSize = 0);
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
	
	void Send(const uint32_t ipAddr, const uint16_t port, Packet&& packet, const std::optional<PacketHeader::FlowRoutingInfo>& rawTxData = std::nullopt, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback = std::nullopt);
	void Run(const std::chrono::milliseconds &duration = std::chrono::milliseconds::max());
	
	void SetRawTx(const FileDescriptor &fd, const PacketHeader& header, const PacketHeader::FlowRoutingInfo& defaultRoute);
	void ClearRawTx();
	bool SetAffinity(int cpu);
	bool SetThreadName(const std::string& name);
	bool SetPriority(int priority);
	bool IsRunning() const { return running; }
	

	ObjectPool<Packet>& GetPacketPool() { return packetPool; }

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
private:
	struct SendBuffer
	{
		//Don't allocate packet on default constructor
		SendBuffer() :
			packet(0)
		{
		}
		
		SendBuffer(uint32_t ipAddr, uint16_t port, const std::optional<PacketHeader::FlowRoutingInfo>& rawTxData, Packet&& packet, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback) :
			ipAddr(ipAddr),
			port(port),
			packet(std::move(packet)),
			rawTxData(rawTxData),
			callback(callback)
		{
		}
		SendBuffer(SendBuffer&& other) :
			ipAddr(other.ipAddr),
			port(other.port),
			packet(std::move(other.packet)),
			rawTxData(other.rawTxData),
			callback(other.callback)
		{
		}
		SendBuffer& operator=(SendBuffer&&) = default;
		//NO copyable
		SendBuffer(const SendBuffer&) = delete;
		SendBuffer& operator=(SendBuffer const&) = delete;
		
		uint32_t ipAddr = 0;
		uint16_t port = 0;
		Packet   packet;
		std::optional<PacketHeader::FlowRoutingInfo> rawTxData;
		std::optional<std::function<void(std::chrono::milliseconds)>> callback;
		
	};
	static const size_t MaxSendingQueueSize;
	static const size_t MaxMultipleSendingMessages;
	static const size_t MaxMultipleReceivingMessages;
	static const size_t PacketPoolSize;
private:
	std::thread	thread;
	State		state		= State::Normal;
	Listener*	listener	= nullptr;
	int		fd		= 0;
	int		pipe[2]		= {FD_INVALID, FD_INVALID};
	pollfd		ufds[2]		= {};
	std::atomic_flag signaled	= ATOMIC_FLAG_INIT;
	volatile bool	running		= false;
	std::chrono::milliseconds now	= 0ms;
	moodycamel::ConcurrentQueue<SendBuffer>	sending;
	moodycamel::ConcurrentQueue<
		std::pair<
			std::function<void(std::chrono::milliseconds)>,
			std::optional<std::function<void(std::chrono::milliseconds)>>
		>
	>  tasks;
	std::multimap<std::chrono::milliseconds,TimerImpl::shared> timers;
	ObjectPool<Packet> packetPool;
	std::optional<RawTx> rawTx;

};

#endif /* EVENTLOOP_H */

