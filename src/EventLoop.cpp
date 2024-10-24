#include "tracing.h"
#include "EventLoop.h"

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <pthread.h>
#include <cmath>

#include "log.h"

#if __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
const size_t EventLoop::MaxMultipleSendingMessages = 1;
const size_t EventLoop::MaxMultipleReceivingMessages = 1;

struct mmsghdr
{
	struct msghdr msg_hdr;	/* Actual message header.  */
	unsigned int msg_len;	/* Number of received or sent bytes for the entry.  */
};

 int sendmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen, int flags)
 {
	 int ret = 0;
	 for (unsigned int len = 0; len<vlen; ++len)
		 ret += (msgvec[len].msg_len = sendmsg(sockfd, &msgvec[len].msg_hdr, flags ))>0;
	 return ret;
 }

 int recvmmsg(int sockfd, struct mmsghdr* msgvec, unsigned int vlen, int flags, struct timespec* timeout);
 {
	 int ret = 0;
	 for (unsigned int len = 0; len < vlen; ++len)
		 ret += (msgvec[len].msg_len = recvmsg(sockfd, &msgvec[len].msg_hdr, flags)) > 0;
	 return ret;
 }
#else
#include <linux/errqueue.h>
#include <sys/eventfd.h>

cpu_set_t* alloc_cpu_set(size_t* size) {
	// the CPU set macros don't handle cases like my Azure VM, where there are 2 cores, but 128 possible cores (why???)
	// hence requiring an oversized 16 byte cpu_set_t rather than the 8 bytes that the macros assume to be sufficient.
	// this is the only way (even documented as such!) to figure out how to make a packet big enough
	unsigned long* packet = nullptr;
	int len = 0;
	do {
		++len;
		delete [] packet;
		packet = new unsigned long[len];
	} while(pthread_getaffinity_np(pthread_self(), len * sizeof(unsigned long), reinterpret_cast<cpu_set_t*>(packet)) == EINVAL);

	*size = len * sizeof(unsigned long);
	return reinterpret_cast<cpu_set_t*>(packet);
}

void free_cpu_set(cpu_set_t* s) {
	delete [] reinterpret_cast<unsigned long*>(s);
}
#endif

EventLoop::EventLoop(std::unique_ptr<Poll> poll) : poll(std::move(poll))
{
	Debug("-EventLoop::EventLoop() [this:%p]\n", this);
}


EventLoop::~EventLoop()
{
	Debug("-EventLoop::~EventLoop() [this:%p]\n", this);
	if (running)
		Stop();
}

bool EventLoop::SetThreadName(std::thread::native_handle_type thread, const std::string& name)
{
	if (reinterpret_cast<void*>(thread) == nullptr) {
		return false;
	}

#if defined(__linux__)
	return !pthread_setname_np(thread, name.c_str());
#endif
	return false;
}

bool EventLoop::SetAffinity(std::thread::native_handle_type thread, int cpu)
{
	if (reinterpret_cast<void*>(thread) == nullptr) {
		return false;
	}

#ifdef THREAD_AFFINITY_POLICY
	if (cpu >= 0)
	{
		thread_affinity_policy_data_t policy = { cpu + 1 };
		return !thread_policy_set(pthread_mach_thread_np(thread), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
	}
	else {
		thread_affinity_policy_data_t policy = { 0 };
		return !thread_policy_set(pthread_mach_thread_np(thread), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
	}

#else
	size_t cpuSize = 0;
	cpu_set_t* cpuSet = alloc_cpu_set(&cpuSize);
	CPU_ZERO_S(cpuSize, cpuSet);

	//Set affinity to the cpu core
	if (cpu >= 0)
		//Set mask
		CPU_SET(cpu, cpuSet);
	else
		//Set affinity for all cpus
		for (size_t j = 0; j < cpuSize; j++)
			CPU_SET(j, cpuSet);

	//Set thread affinity
	bool ret = !pthread_setaffinity_np(thread, cpuSize, cpuSet);

	//Clear cpu mask
	free_cpu_set(cpuSet);

	//Done
	return ret;
#endif

}

bool EventLoop::SetAffinity(int cpu)
{
	//Set event loop thread affinity
	return EventLoop::SetAffinity(thread.native_handle(), cpu);

}

bool EventLoop::SetThreadName(const std::string& name)
{
	return EventLoop::SetThreadName(thread.native_handle(), name);
}

bool EventLoop::SetPriority(int priority)
{
	if (reinterpret_cast<void*>(thread.native_handle()) == nullptr) {
		return false;
	}

	sched_param param = { 
		.sched_priority = priority
	};
	return !pthread_setschedparam(thread.native_handle(), priority ? SCHED_FIFO : SCHED_OTHER ,&param);
}

bool EventLoop::StartWithLoop(std::function<void(void)> loop)
{
	//If already started
	if (running)
		//Stop first
		Stop();
	
	//Log
	TRACE_EVENT("eventloop", "EventLoop::Start(loop)");
	Debug("-EventLoop::Start()\n");
	
	//Block signals to avoid exiting on SIGUSR1
	blocksignals();
	
	exitCode.reset();
	
	//Running
	running = true;
	
	//Start thread and run
	thread = std::thread([loop](){
		loop();
	});
	
	//Done
	return true;
}

bool EventLoop::StartWithFd(int fd)
{
	poll->Clear();
	
	return AddFd(fd) && Start();
}

bool EventLoop::Start()
{
	//If already started
	if (running)
		//Alredy running, Run() doesn't exit so Stop() must be called explicitly
		return Error("-EventLoop::Start() | Already running\n");
	
	//Log
	TRACE_EVENT("eventloop", "EventLoop::Start()");
	Debug("-EventLoop::Start() [this:%p]\n", this);
	
	//Block signals to avoid exiting on SIGUSR1
	blocksignals();
	
	exitCode.reset();
	
	//Running
	running = true;
	
	//Start thread and run
	thread = std::thread([this](){ Run(); });
	
	//Done
	return true;
}

bool EventLoop::Stop()
{
	//Check if running
	if (!running)
		//Nothing to do
		return Error("-EventLoop::Stop() | Already stopped\n");
	
	//Log
	TRACE_EVENT("eventloop", "EventLoop::Stop");
	Debug(">EventLoop::Stop() [this:%p]\n",this);
	
	//Not running
	running = false;
	
	//If it was not external
	if (thread.joinable())
	{
		//Signal the thread this will cause the poll call to exit
		Signal();

		//Nulifi thread
		thread.join();
	}
	
	//Log
	Debug("<EventLoop::Stop()\n");
	
	//Done
	return true;
	
}

void EventLoop::AsyncUnsafe(const std::function<void(std::chrono::milliseconds)>& func)
{
	//UltraDebug(">EventLoop::Async()\n");

	//If not in the same thread
	if (std::this_thread::get_id() != thread.get_id())
	{
		//Create task
		auto task = std::make_pair(func,std::nullopt);

		//Add to pending taks
		tasks.enqueue(std::move(task));

		//Signal the thread this will cause the poll call to exit
		Signal();
	} else {
		//Call now otherwise
		func(GetNow());
	}

	//UltraDebug("<EventLoop::Async()\n");
}

void EventLoop::AsyncUnsafe(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback)
{
	//UltraDebug(">EventLoop::Async()\n");

	//If not in the same thread
	if (std::this_thread::get_id() != thread.get_id())
	{
		//Create task
		auto task = std::make_pair(func, callback);

		//Add to pending taks
		tasks.enqueue(std::move(task));

		//Signal the thread this will cause the poll call to exit
		Signal();
	} else {
		//Call now otherwise
		func(GetNow());
		//Call callback
		callback(GetNow());
	}
	//UltraDebug("<EventLoop::Async()\n");
}


std::future<void> EventLoop::FutureUnsafe(const std::function<void(std::chrono::milliseconds)>& func)
{
	//UltraDebug(">EventLoop::Future()\n");
	
	//As std::functions only allow copiable objets, we have to wrap the promise inside a shared_ptr
	auto promise = std::make_shared<std::promise<void>>();

	//Add an async with callback
	AsyncUnsafe(func, [=](std::chrono::milliseconds) {
		//Resolve promise on callbak
		promise->set_value();
	});

	//UltraDebug("<EventLoop::Future()\n");
	
	//Return the future for the promise
	return promise->get_future();;
}

Timer::shared EventLoop::CreateTimerUnsafe(const std::function<void(std::chrono::milliseconds)>& callback)
{
	//Create timer without scheduling it
	auto timer = std::make_shared<TimerImpl>(*this,0ms,callback);
	//Done
	return std::static_pointer_cast<Timer>(timer);
}

Timer::shared EventLoop::CreateTimerUnsafe(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& callback)
{
	//Timer without repeat
	return CreateTimerUnsafe(ms,0ms,callback);
}

Timer::shared EventLoop::CreateTimerUnsafe(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& callback)
{
	//UltraDebug(">EventLoop::CreateTimer() | CreateTimer in %u\n", ms.count());

	//Create timer
	auto timer = std::make_shared<TimerImpl>(*this,repeat,callback);
	
	//Get next
	auto next = this->Now() + ms;
	
	//Add it async
	AsyncUnsafe([this,timer,next](auto now){
		//Set next tick
		timer->next = next;

		//Add to timer list
		timers.emplace(next, timer);
	});
	
	//Done
	return std::static_pointer_cast<Timer>(timer);
}

void EventLoop::TimerImpl::Cancel()
{
	//Add it async
	loop.AsyncUnsafe([timer = shared_from_this()](auto now){
		//Remove us
		timer->loop.CancelTimer(timer);
	});
}

void EventLoop::TimerImpl::Again(const std::chrono::milliseconds& ms)
{
	//UltraDebug(">EventLoop::Again() | Again triggered in %u\n",ms.count());
	
	//Get next
	auto next = loop.Now() + ms;
	
	//Reschedule it async
	loop.AsyncUnsafe([timer = shared_from_this(),next](auto now){
		//Remove us
		timer->loop.CancelTimer(timer);

		//Set next tick
		timer->next = next;
		
		//Add to timer list
		timer->loop.timers.emplace(next, timer);
	});
	
	//UltraDebug("<EventLoop::Again() | timer triggered at %llu\n",next.count());
}

void EventLoop::TimerImpl::Repeat(const std::chrono::milliseconds& repeat)
{
	Reschedule(0ms, repeat);
}

void EventLoop::TimerImpl::Reschedule(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat)
{
	//UltraDebug(">EventLoop::TimerImpl::Reschedule() | in %u repeat %u\n", ms.count(), repeat.count());

	//Get next
	auto next = loop.Now() + ms;

	//Reschedule it async
	loop.AsyncUnsafe([timer = shared_from_this(), next, repeat](auto now){
		//Remove us
		timer->loop.CancelTimer(timer);

		//Set next tick
		timer->next = next;
		//Update repeat interval
		timer->repeat = repeat;
		
		//Add to timer list
		timer->loop.timers.emplace(next, timer);
	});
}

void EventLoop::CancelTimer(TimerImpl::shared timer)
{
	//UltraDebug(">EventLoop::CancelTimer() \n");

	//We don't have to repeat this
	timer->repeat = 0ms;
	
	//If not scheduled
	if (!timer->next.count())
		//Nothing
		return;
	
	//Get all timers at that tick
	auto result = timers.equal_range(timer->next);
	
	//Reset next tick
	timer->next = 0ms;

	// Iterate over the range
	for (auto it = result.first; it != result.second; ++it)
	{
		//If it is the same impl
		if (it->second.get()==timer.get())
		{
			//Remove this one
			timers.erase(it);
			//Found
			break;
		}
	}
	//UltraDebug("<EventLoop::CancelTimer() \n");
}

const std::chrono::milliseconds EventLoop::Now()
{
	//Get new now and store in cache
	return now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}

bool EventLoop::AddFd(int fd, std::optional<uint16_t> eventMask)
{
	if (!poll->AddFd(fd)) return false;
	
	if (eventMask.has_value())
	{
		return poll->SetEventMask(fd, *eventMask);
	}
	
	return true;
}

bool EventLoop::RemoveFd(int fd)
{
	return poll->RemoveFd(fd);
}

void EventLoop::ForEachFd(const std::function<void(int)>& func)
{
	poll->ForEachFd([&func](int fd) {
		func(fd);
		return std::nullopt;
	});
}

void EventLoop::Signal()
{
	TRACE_EVENT("eventloop", "EventLoop::Signal");

	//If we are in the same thread or already signaled and pipe is ok
	if (std::this_thread::get_id()==thread.get_id())
		//No need to do anything
		return;
		
	poll->Signal();
}

void EventLoop::Run(const std::chrono::milliseconds &duration)
{
	//Log(">EventLoop::Run() | [%p,running:%d,duration:%llu]\n",this,running,duration.count());
	OnLoopEnter();
	
	signal(SIGIO,[](int){});
	
	//Get now
	auto now = Now();
	
	//calculate until when
	auto until = duration < std::chrono::milliseconds::max() ? now + duration : std::chrono::milliseconds::max();
		
	//Run until ended
	while(running && !exitCode.has_value() && now<=until)
	{
		//TRACE_EVENT("eventloop", "EventLoop::Run::Iteration");
		
		poll->ForEachFd([this](int fd) {
			
			auto events = GetPollEventMask(fd);
			if (events)
				poll->SetEventMask(fd, *events);
		});
		
		//Until signaled or one each 10 seconds to prevent deadlocks
		int timeout = GetNextTimeout(10E3, until);

		//UltraDebug(">EventLoop::Run() | poll timeout:%d timers:%d tasks:%d size:%d\n",timeout,timers.size(),tasks.size_approx(), sizeof(ufds) / sizeof(pollfd));

		//Wait for events
		if (auto error = poll->Wait(timeout) != 0)
		{
			Error("Event poll wait failed. code: %d\n", error);
			SetStopping(ToUType(PredefinedExitCode::WaitError));
			break;
		}
		
		if (!running) break;
		
		//Update now
		now = Now();
		
		poll->ForEachFd([this](int fd) {
			auto [events, error] = poll->GetEvents(fd);
			if (error)
			{
				OnPollError(fd, error);
			}
			else if (events & Poll::Event::In)
			{
				OnPollIn(fd);
			}
			else if (events & Poll::Event::Out)
			{
				OnPollOut(fd);
			}
		});
		
		//Process pendint tasks
		ProcessTasks(now);

		//Timers triggered
		ProcessTriggers(now);
		
		//Update now
		now = Now();
	}
	
	//Run queued tasks before exiting
	ProcessTasks(now);

	OnLoopExit(exitCode.has_value() ? *exitCode : 0);
	//Log("<EventLoop::Run()\n");
}

int EventLoop::GetNextTimeout(int defaultTimeout, const std::chrono::milliseconds& until) const
{
	int timeout = defaultTimeout;

	//Check if we have any pending task to wait or exit poll inmediatelly
	if (tasks.size_approx())
	{
		//No wait
		timeout = 0;
	}
	//If we have any timer or a timeout
	else if (timers.size())
	{
		//Get first timer in  queue
		auto next = std::min(timers.cbegin()->first, until);
		//Override timeout
		timeout = next > now ? std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count() : 0;
	}
	//If we have a maximum duration
	else if (until != std::chrono::milliseconds::max())
	{
		//Override timeout
		timeout = until > now ? std::chrono::duration_cast<std::chrono::milliseconds>(until - now).count() : 0;
	}

	return std::min(timeout, defaultTimeout);
}

void EventLoop::ProcessTasks(const std::chrono::milliseconds& now)
{
	//Run queued task
	TRACE_EVENT_BEGIN("eventloop", "EventLoop::ProcessTasks");
	std::pair<std::function<void(std::chrono::milliseconds)>,std::optional<std::function<void(std::chrono::milliseconds)>>> task;
	//Get all pending taks
	while (tasks.try_dequeue(task))
	{
		//UltraDebug(">EventLoop::Run() | task pending\n");
		//Execute it
		task.first(now);
		//If we had a callback
		if (task.second.has_value())
			//Run now
			task.second.value()(now);
		//UltraDebug("<EventLoop::Run() | task run\n");
	}
	TRACE_EVENT_END("eventloop");
}

void EventLoop::ProcessTriggers(const std::chrono::milliseconds& now)
{
	//Run triggered timers
	TRACE_EVENT_BEGIN("eventloop", "EventLoop::ProcessTimers");
	std::vector<TimerImpl::shared> triggered;
	//Get all timers to process in this lop
	for (auto it = timers.begin(); it != timers.end(); )
	{
		//Check it we are still on the time
		if (it->first > now)
			//It is yet to come
			break;
		//Get timer
		triggered.push_back(std::move(it->second));
		//Remove from the list
		it = timers.erase(it);
	}

	//Now process all timers triggered
	for (auto timer : triggered)
	{
		//Get scheduled time
		auto scheduled = timer->next;

		//UltraDebug(">EventLoop::Run() | timer [%s] triggered at ll%u scheduled at %lld\n",timer->GetName().c_str(),now.count(),scheduled.count());
	
		//We are executing
		timer->next = 0ms;

		//Timer triggered
		TRACE_EVENT_BEGIN("eventloop", "EventLoop::ProcessTriggers::ExecuteTimer", "name", timer->GetName().c_str());

		//Execute it
		timer->callback(now);

		//Timer ended
		TRACE_EVENT_END("eventloop");

		//If we have to reschedule it again
		if (timer->repeat.count() && !timer->next.count())
		{
			//UltraDebug("-EventLoop::Run() | timer rescheduled\n");
			//Set next
			timer->next = scheduled + timer->repeat;
			//If the event loop has frozen and the next one is still in the past
			if (timer->next<now)
				//Set it to the next one in the future
				timer->next = timer->next + std::chrono::duration_cast<std::chrono::milliseconds>(std::ceil((now - timer->next) / timer->repeat) * timer->repeat);
			//Schedule
			timers.emplace(timer->next, timer);
		}
		//UltraDebug("<EventLoop::Run() | timer run \n");
	}
	TRACE_EVENT_END("eventloop");
}
