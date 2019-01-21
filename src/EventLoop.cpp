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
#include <cassert>
#include <pthread.h>

#include "log.h"

EventLoop::EventLoop(Listener &listener) :
	listener(listener)
{
}

EventLoop::~EventLoop()
{
}

bool EventLoop::Start(int fd)
{
	//If already started
	if (thread.get_id()!=std::thread::id())
		//Alredy running
		return false;
	
	//Create pipe
	if (::pipe(pipe)==-1)
		//Error
		return false;
	
	//Set non blocking
	int flags = fcntl(pipe[0], F_GETFL, 0);
	fcntl(pipe[0], F_SETFD , flags | O_NONBLOCK);
	flags = fcntl(pipe[1], F_GETFL, 0);
	fcntl(pipe[1], F_SETFD , flags | O_NONBLOCK);
	
	//Store socket
	this->fd = fd;
	
	//Running
	running = true;

	//Start thread and run
	thread = std::thread([this](){ Run(); });
	
	//Done
	return true;
}

bool EventLoop::Stop()
{
	//Check thred
	if (thread.get_id()==std::thread::id())
		//Nothing to do
		return false;
	
	//Not running
	running = false;
	
	//Signal the pthread this will cause the poll call to exit
	Signal();
	
	//Nulifi thread
	thread.join();
	
	//Close pipe
	close(pipe[0]);
	close(pipe[1]);
	
	//Empyt pipe
	pipe[0] = pipe[1] = 0;
	
	//Done
	return true;
	
}

void EventLoop::Send(const uint32_t ipAddr, const uint16_t port, Buffer&& buffer)
{
	//Create send buffer
	SendBuffer send = {ipAddr, port, std::move(buffer)};
	
	//Move it back to sending queue
	sending.enqueue(std::move(send));
	
	//Signal the pthread this will cause the poll call to exit
	Signal();
}

void EventLoop::Async(std::function<void(std::chrono::milliseconds)> func)
{
	//Add to pending taks
	tasks.enqueue(func);
	
	//Signal the pthread this will cause the poll call to exit
	Signal();
}

Timer::shared EventLoop::CreateTimer(const std::chrono::milliseconds& ms, std::function<void(std::chrono::milliseconds)> callback)
{
	//Timer without repeat
	return CreateTimer(ms,0ms,callback);
}

Timer::shared EventLoop::CreateTimer(const std::chrono::milliseconds& ms, const std::chrono::milliseconds& repeat, std::function<void(std::chrono::milliseconds)> callback)
{
	//Create timer
	auto timer = std::make_shared<TimerImpl>(*this,repeat,callback);
	
	//Get next
	auto next = this->GetNow() + ms;
	
	//Add it async
	Async([this,timer,next](...){
		//Set next tick
		timer->next = next;

		//Add to timer list
		timers.insert({next, timer});
	});
	
	//Done
	return std::static_pointer_cast<Timer>(timer);
}

void EventLoop::TimerImpl::Cancel()
{
	TimerImpl::shared timer = shared_from_this();
	//Add it async
	loop.Async([timer](...){
		//Remove us
		timer->loop.CancelTimer(timer);
	});
}

void EventLoop::TimerImpl::Again(const std::chrono::milliseconds& ms)
{
	//Get next
	auto next = loop.GetNow() + ms;
	
	//Reschedule it async
	loop.Async([timer = shared_from_this(),next](...){
		//Remove us
		timer->loop.CancelTimer(timer);

		//Set next tick
		timer->next = next;

		//Add to timer list
		timer->loop.timers.insert({next, timer});
	});
}

void EventLoop::CancelTimer(std::shared_ptr<TimerImpl> timer)
{
	//TODO: Asert we are on same thread than loop

	//We don't have to repeat this
	timer->repeat = 0ms;
	
	//If not scheduled
	if (timer->next.count())
		//Nothing
		return;
	
	//Reset next tick
	timer->next = 0ms;
			
	//Get all timers at that tick
	auto result = timers.equal_range(timer->next);

	// Iterate over the range
	for (auto it = result.first; it != result.second; it++)
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
}

const std::chrono::milliseconds EventLoop::Now()
{
	//Get new now and store in cache
	return now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}

void EventLoop::Signal()
{
	//If we are in the same thread
	if (std::this_thread::get_id()==thread.get_id())
		//No need to do anything
		return;
	//Write a single char to pipe
	write(pipe[1],".",1);
}

void EventLoop::Run()
{
	Log(">EventLoop::Run() | [%p]\n",this);
	
	//Recv data
	uint8_t data[MTU] ZEROALIGNEDTO32;
	size_t  size = MTU;
	sockaddr_in from;
	memset(&from,0,sizeof(sockaddr_in));
	
	//Pending data
	SendBuffer item;
	bool pending = false;
	
	//Set values for polling
	ufds[0].fd = fd;
	ufds[1].fd = pipe[0];
	//Set to wait also for write events on pipe for signaling
	ufds[1].events = POLLIN | POLLERR | POLLHUP;

	//Set non blocking so we can get an error when we are closed by end
	int fsflags = fcntl(fd,F_GETFL,0);
	fsflags |= O_NONBLOCK;
	fcntl(fd,F_SETFL,fsflags);
	
	//Catch all IO errors and do nothing
	signal(SIGIO,[](int){});
	
	//Run until ended
	while(running)
	{
		//If we have anything to send set to wait also for write events
		ufds[0].events =sending.size_approx() ? POLLIN | POLLOUT | POLLERR | POLLHUP : POLLIN | POLLERR | POLLHUP;
		
		//Do not wait
		int timeout = 0;
		
		//Get now
		auto now = GetNow();
		
		//Get next timer in  queue
		auto next = timers.size() ? timers.begin()->first : 0ms;
		
		//If there are no pending tasks
		if (!tasks.size_approx())
			//Override timeout
			timeout = now > next ? std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count() : -1;
		
		
		//Wait for events
		poll(ufds,sizeof(ufds)/sizeof(pollfd),timeout);
			
		//Check for cancel
		if ((ufds[0].revents & POLLHUP) || (ufds[0].revents & POLLERR) || (ufds[1].revents & POLLHUP) || (ufds[1].revents & POLLERR))
		{
			//Error
			Log("-EventLoop::Run() | Pool error event [%d]\n",ufds[0].revents);
			//Exit
			break;
		}
		
		//Read first from signal pipe
		if (ufds[1].revents & POLLIN)
		{
			//Remove pending data
			while (read(pipe[0],data,size)>0) 
			{
				//DO nothing
			}
		}
		
		//Read first
		if (ufds[0].revents & POLLIN)
		{
			//UltraDebug("-EventLoop::Run() | ufds[0].revents & POLLIN\n",n);
			//Len
			uint32_t fromLen = sizeof(from);
			//Leemos del socket
			int len = recvfrom(fd,data,size,MSG_DONTWAIT,(sockaddr*)&from,&fromLen);
			//Run callback
			listener.OnRead(ufds[0].fd,data,len,ntohl(from.sin_addr.s_addr),ntohs(from.sin_port));
		}
		
		//Check read is possible
		if (ufds[0].revents & POLLOUT)
		{
			//UltraDebug("-EventLoop::Run() | ufds[0].revents & POLLOUT\n",n);
			
			//Check if we had any pending item
			if (!pending)
				//Get first
				pending = sending.try_dequeue(item);
			
			//Now send all that we can
			while (pending)
			{
				//Create send address
				sockaddr_in to;
				memset(&to,0,sizeof(sockaddr_in));
				to.sin_family	   = AF_INET;
				to.sin_addr.s_addr = htonl(item.ipAddr);
				to.sin_port	   = htons(item.port);

				//Send it
				int ret = sendto(fd,item.buffer.GetData(),item.buffer.GetSize(),MSG_DONTWAIT,(sockaddr*)&to,sizeof(to));
				
				//It we have an error
				if (ret<0)
					//Retry again later
					break;
				//Get next item
				pending = sending.try_dequeue(item);
			}
			
		}
		
		//Run queued task
		std::function<void(std::chrono::milliseconds)> task;
		//Get all pending taks
		while (tasks.try_dequeue(task))
		{
			//Update now
			auto now = Now();
			//Execute it
			task(now);
		}
		
		//Proccess all timers
		for (auto it = timers.begin(); it!=timers.end(); )
		{
			//Update now
			auto now = Now();
			//Check it we are still on the time
			if (it->first>now)
				//It is yet to come
				break;
			//Get timer
			auto timer = it->second;
			//We are executing
			timer->next = 0ms;
			//Execute it
			timer->callback(now);
			//Remove from the list
			it = timers.erase(it);
			//If we have to reschedule it again
			if (timer->repeat.count())
			{
				//Set nest
				timer->next = now + timer->repeat;
				//Schedule
				timers.insert({timer->next, timer});
			}
		}
	}

	Log("<EventLoop::Run()\n");
}
