#include "PollSignalling.h"

#include "log.h"

#include <sys/eventfd.h>


PollSignalling::PollSignalling()
{
	int pipe[2] = {FD_INVALID, FD_INVALID};
	
#if __APPLE__
	//Create pipe
	if (::pipe(pipe)==-1)
	{
		//Error
		throw std::runtime_error("Failed to create pipe");
	}
	
	//Set non blocking
	fcntl(pipe[0], F_SETFL , O_NONBLOCK);
	fcntl(pipe[1], F_SETFL , O_NONBLOCK);
	
#else
	pipe[0] = pipe[1] = eventfd(0, EFD_NONBLOCK);
#endif	

	pipeFds[0] = FileDescriptor(pipe[0]);
	pipeFds[1] = FileDescriptor(pipe[1]);

	//Check values
	if (pipe[0]==FD_INVALID || pipe[1]==FD_INVALID)
	{
		//Error
		Error("-PollSignalling::PollSignalling | could not start pipe [errno:%d]\n",errno);
		throw std::runtime_error("Failed to create pipe");
	}
	
	//We are not signaled anymore
	signaled.clear();
}

void PollSignalling::Signal()
{	
	uint64_t one = 1;

	if (signaled.test_and_set() || !pipeFds[1].isValid())
		//No need to do anything
		return;
			
	//We have signaled it above
	//worst case scenario is that race happens between this to points
	//and that we signal it twice
	
	//Write to tbe pipe, and assign to one to avoid warning in compile time
	one = write(pipeFds[1],(uint8_t*)&one,sizeof(one));
}

void PollSignalling::ClearSignal()
{
	if (pipeFds[0].isValid())
	{
		uint64_t data = 0;
		//Remove pending data on signal pipe
		while (read(pipeFds[0], &data, sizeof(uint64_t)) > 0)
		{
			//DO nothing
		}
	}
	
	//We are not signaled anymore
	signaled.clear();
}
	
