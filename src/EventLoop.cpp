#include "tracing.h"
#include "EventLoop.h"

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <pthread.h>
#include <cmath>

#include "log.h"

const size_t EventLoop::MaxSendingQueueSize = 64*1024;
const size_t EventLoop::PacketPoolSize = 1024;


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

const size_t EventLoop::MaxMultipleSendingMessages = 128;
const size_t EventLoop::MaxMultipleReceivingMessages = 128;

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

EventLoop::EventLoop(Listener *listener, uint32_t packetPoolSize) :
	listener(listener),
	packetPool(packetPoolSize ? packetPoolSize : PacketPoolSize)
{
	Debug("-EventLoop::EventLoop() [this:%p,packetPoolSize:%lu]\n", this, packetPool.size());
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
#ifdef 	SO_INCOMING_CPU
	//If got socket
	if (fd)
		//Set incoming socket cpu affinity
		(void)setsockopt(fd, SOL_SOCKET, SO_INCOMING_CPU, &cpu, sizeof(cpu));
#endif

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

void EventLoop::SetRawTx(const FileDescriptor& fd, const PacketHeader& header, const PacketHeader::FlowRoutingInfo& defaultRoute)
{
	rawTx.emplace(fd, header, defaultRoute);
}


void EventLoop::ClearRawTx()
{
	rawTx.reset();
}

bool EventLoop::Start(std::function<void(void)> loop)
{
	//If already started
	if (running)
		//Stop first
		Stop();
	
	//Log
	TRACE_EVENT("eventloop", "EventLoop::Start(loop)");
	Debug("-EventLoop::Start()\n");
	
#if __APPLE__
	//Create pipe
	if (::pipe(pipe)==-1)
		//Error
		return false;
	
	//Set non blocking
	fcntl(pipe[0], F_SETFL , O_NONBLOCK);
	fcntl(pipe[1], F_SETFL , O_NONBLOCK);
	
#else
	pipe[0] = pipe[1] = eventfd(0, EFD_NONBLOCK);
#endif	
	//Check values
	if (pipe[0]==FD_INVALID || pipe[1]==FD_INVALID)
		//Error
		return Error("-EventLoop::Start() | could not start pipe [errno:%d]\n",errno);
			
	//Store socket
	this->fd = FD_INVALID;
	
	//Running
	running = true;

	//Not signaled
	signaled.clear();
	
	//Start thread and run
	thread = std::thread(loop);
	
	//Done
	return true;
}

bool EventLoop::Start(int fd)
{
	//If already started
	if (running)
		//Alredy running, Run() doesn't exit so Stop() must be called explicitly
		return Error("-EventLoop::Start() | Already running\n");
	
	//Log
	TRACE_EVENT("eventloop", "EventLoop::Start(fd)");
	Debug("-EventLoop::Start() [fd:%d,this:%p]\n",fd,this);
	
#if __APPLE__
	//Create pipe
	if (::pipe(pipe)==-1)
		//Error
		return false;
	
	//Set non blocking
	fcntl(pipe[0], F_SETFL , O_NONBLOCK);
	fcntl(pipe[1], F_SETFL , O_NONBLOCK);
	
#else
	pipe[0] = pipe[1] = eventfd(0, EFD_NONBLOCK);
#endif	
	//Check values
	if (pipe[0]==FD_INVALID || pipe[1]==FD_INVALID)
		//Error
		return Error("-EventLoop::Start() | could not start pipe [errno:%d]\n",errno);
	
	//Store socket
	this->fd = fd;
	
	//Running
	running = true;

	//Not signaled
	signaled.clear();
	
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
	Debug(">EventLoop::Stop() [fd:%d,this:%p]\n",fd,this);
	
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
	
	//Close pipe
	if (pipe[0]!=FD_INVALID) close(pipe[0]);
	if (pipe[1]!=FD_INVALID) close(pipe[1]);
	
	//Empyt pipe
	pipe[0] = pipe[1] = FD_INVALID;
	
	//Log
	Debug("<EventLoop::Stop() [fd:%d]\n",fd);
	
	//Done
	return true;
	
}

void EventLoop::Send(const uint32_t ipAddr, const uint16_t port, Packet&& packet, const std::optional<PacketHeader::FlowRoutingInfo>& rawTxData, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback)
{
	TRACE_EVENT("eventloop", "EventLoop::Send", "packet_size", packet.GetSize());

	//Get approximate queued size
	auto aprox = sending.size_approx();
	
	//Check if there is too much in the queue already
	if (aprox>MaxSendingQueueSize)
	{
		//Check state
		if (state!=State::Overflown)
		{
			//We are overflowing
			state = State::Overflown;
			//Log
			Error("-EventLoop::Send() | sending queue overflown [aprox:%lu]\n",aprox);
		}
		//Do not enqueue more
		return;
	} else if (aprox>MaxSendingQueueSize/2 && state==State::Normal) {
		//We are lagging behind
		state = State::Lagging;
		//Log
		Error("-EventLoop::Send() | sending queue lagging behind [aprox:%lu]\n",aprox);
	} else if (aprox<MaxSendingQueueSize/4 && state!=State::Normal)  {
		//We are normal again
		state = State::Normal;
		//Log
		Log("-EventLoop::Send() | sending queue back to normal [aprox:%lu]\n",aprox);
	}
	
	//Create send packet
	SendBuffer send = {ipAddr, port, rawTxData, std::move(packet), callback};
	
	//Move it back to sending queue
	sending.enqueue(std::move(send));
	
	//Signal the thread this will cause the poll call to exit
	Signal();
}

void EventLoop::Async(const std::function<void(std::chrono::milliseconds)>& func)
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

void EventLoop::Async(const std::function<void(std::chrono::milliseconds)>& func, const std::function<void(std::chrono::milliseconds)>& callback)
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


std::future<void> EventLoop::Future(const std::function<void(std::chrono::milliseconds)>& func)
{
	//UltraDebug(">EventLoop::Future()\n");
	
	//As std::functions only allow copiable objets, we have to wrap the promise inside a shared_ptr
	auto promise = std::make_shared<std::promise<void>>();

	//Add an async with callback
	Async(func, [=](std::chrono::milliseconds) {
		//Resolve promise on callbak
		promise->set_value();
	});

	//UltraDebug("<EventLoop::Future()\n");
	
	//Return the future for the promise
	return promise->get_future();;
}

Timer::shared EventLoop::CreateTimer(const std::function<void(std::chrono::milliseconds)>& callback)
{
	//Create timer without scheduling it
	auto timer = std::make_shared<TimerImpl>(*this,0ms,callback);
	//Done
	return std::static_pointer_cast<Timer>(timer);
}

Timer::shared EventLoop::CreateTimer(const std::chrono::milliseconds& ms, const std::function<void(std::chrono::milliseconds)>& callback)
{
	//Timer without repeat
	return CreateTimer(ms,0ms,callback);
}

Timer::shared EventLoop::CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, const std::function<void(std::chrono::milliseconds)>& callback)
{
	//UltraDebug(">EventLoop::CreateTimer() | CreateTimer in %u\n", ms.count());

	//Create timer
	auto timer = std::make_shared<TimerImpl>(*this,repeat,callback);
	
	//Get next
	auto next = this->Now() + ms;
	
	//Add it async
	Async([this,timer,next](auto now){
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
	loop.Async([timer = shared_from_this()](auto now){
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
	loop.Async([timer = shared_from_this(),next](auto now){
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
	loop.Async([timer = shared_from_this(), next, repeat](auto now){
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

void EventLoop::Signal()
{
	TRACE_EVENT("eventloop", "EventLoop::Signal");
	//UltraDebug("-EventLoop::Signal()\r\n");
	uint64_t one = 1;
	
	//If we are in the same thread or already signaled and pipe is ok
	if (std::this_thread::get_id()==thread.get_id() || signaled.test_and_set() || pipe[1] == FD_INVALID)
		//No need to do anything
		return;
	
	//We have signaled it above
	//worst case scenario is that race happens between this to points
	//and that we signal it twice
	
	
	//Write to tbe pipe, and assign to one to avoid warning in compile time
	one = write(pipe[1],(uint8_t*)&one,sizeof(one));
}

void EventLoop::Run(const std::chrono::milliseconds &duration)
{
	//Log(">EventLoop::Run() | [%p,running:%d,duration:%llu]\n",this,running,duration.count());
	
	//Recv data
// Ignore coverity error: Local variable "datas" uses 192000 bytes of stack space, which exceeds the maximum single use of 20000 bytes.
// coverity[stack_use_local_overflow]
	uint8_t datas[MaxMultipleReceivingMessages][MTU] ZEROALIGNEDTO32;
	size_t  size = MTU;
	
	//UDP send flags
	uint32_t flags = MSG_DONTWAIT;
	
	//Pending data
	std::vector<SendBuffer> items;
	
	//Set values for polling
	ufds[0].fd = fd;
	ufds[1].fd = pipe[0];
	//Set to wait also for write events on pipe for signaling
	ufds[1].events = POLLIN | POLLERR | POLLHUP;

	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(fd,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	(void)fcntl(fd,F_SETFL,fsflags);


	//Catch all IO errors and do nothing
	signal(SIGIO,[](int){});
	
	//Get now
	auto now = Now();
	
	//calculate until when
	auto until = duration < std::chrono::milliseconds::max() ? now + duration : std::chrono::milliseconds::max();
		
	//Run until ended
	while(running && now<=until)
	{
		//TRACE_EVENT("eventloop", "EventLoop::Run::Iteration");
		//If we have anything to send set to wait also for write events
		ufds[0].events = sending.size_approx() ? POLLIN | POLLOUT | POLLERR | POLLHUP : POLLIN | POLLERR | POLLHUP;
		//Clear readed events
		ufds[0].revents = 0;
		ufds[1].revents = 0;
		
		//Until signaled or one each 10 seconds to prevent deadlocks
		int timeout = GetNextTimeout(10E3, until);

		//UltraDebug(">EventLoop::Run() | poll timeout:%d timers:%d tasks:%d size:%d\n",timeout,timers.size(),tasks.size_approx(), sizeof(ufds) / sizeof(pollfd));
		
		//Wait for events
		{
			//TRACE_EVENT("eventloop", "poll", "timeout", timeout);
			(void)poll(ufds,sizeof(ufds)/sizeof(pollfd),timeout);
		}
		
		//Update now
		now = Now();
		
		//UltraDebug("<EventLoop::Run() | poll timeout:%d timers:%d tasks:%d\n",timeout,timers.size(),tasks.size_approx());

		//Check for cancel
		if ((ufds[0].revents & POLLHUP) || (ufds[0].revents & POLLERR) || (ufds[1].revents & POLLHUP) || (ufds[1].revents & POLLERR))
		{
			//Error
			Log("-EventLoop::Run() | Pool error event [revents:%d,errno:%d]\n",ufds[0].revents,errno);
			//Exit
			break;
		}
		
		//Read first
		if (ufds[0].revents & POLLIN)
		{
			struct sockaddr_in froms[MaxMultipleReceivingMessages] = {};
			struct mmsghdr messages[MaxMultipleReceivingMessages] = {};
			struct iovec iovs[MaxMultipleReceivingMessages][1] = {{}};

			TRACE_EVENT("eventloop", "EventLoop::Run::ProcessIn");
			//UltraDebug("-EventLoop::Run() | ufds[0].revents & POLLIN\n");

			//Reserve space
			items.reserve(MaxMultipleSendingMessages);

			//For each msg
			for (size_t i = 0; i < MaxMultipleReceivingMessages; i++)
			{	
				//IO buffer
				auto& iov = iovs[i];
				iov[0].iov_base = datas[i];
				iov[0].iov_len = size;

				//Recv address
				sockaddr_in& from = froms[i];

				//Message
				auto& message = messages[i].msg_hdr;
				message.msg_name = (sockaddr*)&from;
				message.msg_namelen = sizeof(from);
				message.msg_iov = iov;
				message.msg_iovlen = 1;
				message.msg_control = 0;
				message.msg_controllen = 0;
			}

			//Read from socket
			int len = recvmmsg(fd, messages, MaxMultipleReceivingMessages, flags, nullptr);

			//If we got listener
			if (listener)
				//for each one
				for (int i = 0; i < len && i < MaxMultipleReceivingMessages; i++)
					//double check
					if (messages[i].msg_len)
						//Run callback
						listener->OnRead(ufds[0].fd, datas[i], messages[i].msg_len, ntohl(froms[i].sin_addr.s_addr), ntohs(froms[i].sin_port));
		}
		
		//Check read is possible
		if (ufds[0].revents & POLLOUT)
		{
			//Multiple messages struct
			struct mmsghdr messages[MaxMultipleSendingMessages] = {};
			struct sockaddr_in tos[MaxMultipleSendingMessages] = {};
			struct iovec iovs[MaxMultipleSendingMessages][1] = {{}};

			TRACE_EVENT("eventloop", "EventLoop::Run::ProcessOut");
			//UltraDebug("-EventLoop::Run() | ufds[0].revents & POLLOUT\n");

			
			//Now send all that we can
			while (items.size()<MaxMultipleSendingMessages)
			{
				//Get current item
				SendBuffer item;
				
				//Get next item
				if (!sending.try_dequeue(item))
					break;
				
				//Move
				items.emplace_back(std::move(item));
			}
			
			//actual messages dequeued
			uint32_t len = 0;
			
			//For each item
			for (auto& item : items)
			{
				//IO buffer
				auto& iov		= iovs[len];

				//Message
				msghdr& message		= messages[len].msg_hdr;
				message.msg_name	= nullptr;
				message.msg_namelen	= 0;
				message.msg_iov		= iov;
				message.msg_iovlen	= 1;
				message.msg_control	= 0;
				message.msg_controllen	= 0;

				if (!this->rawTx) {
					//Send address
					sockaddr_in& to		= tos[len];
					to.sin_family		= AF_INET;
					to.sin_addr.s_addr	= htonl(item.ipAddr);
					to.sin_port		= htons(item.port);

					message.msg_name	= (sockaddr*) & to;
					message.msg_namelen	= sizeof (to);
				} else {
					//Packet header
					auto& candidateData = item.rawTxData ? *item.rawTxData : this->rawTx->defaultRoute;
					PacketHeader::PrepareHeader(this->rawTx->header, item.ipAddr, item.port, candidateData, item.packet);
					item.packet.PrefixData((uint8_t*) &this->rawTx->header, sizeof(this->rawTx->header));
				}

				//Set packet data
				iov[0].iov_base		= item.packet.GetData();
				iov[0].iov_len		= item.packet.GetSize();
				
				//Reset message len
				messages[len].msg_len	= 0;
				
				//Next
				len++;
			}
			
			//Send them
			int sendFd = this->rawTx ? this->rawTx->fd : fd;
			{
				TRACE_EVENT("eventloop", "sendmmsg", "fd", fd, "vlen", len);
				sendmmsg(sendFd, messages, len, flags);
			}
			
			//Update now
			now = Now();

			//First
			auto it = items.begin();
			//Retry
			std::vector<SendBuffer> retry;
			//check each mesasge
			for (uint32_t i = 0; i<len && it!=items.end(); ++i, ++it)
			{
				//If we are in normal state and we can retry a failed message
				if (!messages[i].msg_len && state==State::Normal && (errno==EAGAIN || errno==EWOULDBLOCK))
				{
					//Retry it
					retry.emplace_back(std::move(*it));
				} else {
					//Move packet buffer back to the pool
					packetPool.release(std::move(it->packet));
					//If we had a callback
					if (it->callback)
						//Set sending time
						it->callback.value()(now);
				}
			}
			//Clear items
			items.clear();
			//Copy elements to retry
			std::move(retry.begin(), retry.end(), std::back_inserter(items));
		}
		
		//Process pendint tasks
		ProcessTasks(now);

		//Timers triggered
		ProcessTriggers(now);
		
		//Read first from signal pipe
		if (ufds[1].revents & POLLIN)
			//Clear signal flag
			ClearSignal();
		
		//Update now
		now = Now();
	}
	
	//Run queued tasks before exiting
	ProcessTasks(now);

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

void EventLoop::ClearSignal()
{
	uint64_t data = 0;
	//Remove pending data on signal pipe
	while (read(pipe[0], &data, sizeof(uint64_t)) > 0)
	{
		//DO nothing
	}
	//We are not signaled anymore
	signaled.clear();
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

