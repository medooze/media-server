#include "tracing.h"
#include "NetEventLoop.h"

#include "log.h"

NetEventLoop::NetEventLoop(Listener* listener, uint32_t packetPoolSize) :
	EventLoop(),
	listener(listener),
	packetPool(packetPoolSize ? packetPoolSize : PacketPoolSize)
{
	Debug("-NetEventLoop::NetEventLoop() [this:%p,packetPoolSize:%lu]\n", this, packetPool.size());
}

void NetEventLoop::SetRawTx(const FileDescriptor& fd, const PacketHeader& header, const PacketHeader::FlowRoutingInfo& defaultRoute)
{
	rawTx.emplace(fd, header, defaultRoute);
}

void NetEventLoop::ClearRawTx()
{
	rawTx.reset();
}

bool NetEventLoop::SetAffinity(int cpu)
{
#ifdef 	SO_INCOMING_CPU
	//If got socket
	EventLoop::ForEachFd([cpu](auto pfd) {
		if (pfd != FD_INVALID)
			//Set incoming socket cpu affinity
			(void)setsockopt(pfd, SOL_SOCKET, SO_INCOMING_CPU, &cpu, sizeof(cpu));
	});
#endif

	//Set event loop thread affinity
	return EventLoop::SetAffinity(cpu);

}

void NetEventLoop::Send(const uint32_t ipAddr, const uint16_t port, Packet&& packet, const std::optional<PacketHeader::FlowRoutingInfo>& rawTxData, const std::optional<std::function<void(std::chrono::milliseconds)>>& callback)
{
	TRACE_EVENT("neteventloop", "NetEventLoop::Send", "packet_size", packet.GetSize());

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
			Error("-NetEventLoop::Send() | sending queue overflown [aprox:%lu]\n",aprox);
		}
		//Do not enqueue more
		return;
	} else if (aprox>MaxSendingQueueSize/2 && state==State::Normal) {
		//We are lagging behind
		state = State::Lagging;
		//Log
		Error("-NetEventLoop::Send() | sending queue lagging behind [aprox:%lu]\n",aprox);
	} else if (aprox<MaxSendingQueueSize/4 && state!=State::Normal)  {
		//We are normal again
		state = State::Normal;
		//Log
		Log("-NetEventLoop::Send() | sending queue back to normal [aprox:%lu]\n",aprox);
	}
	
	//Create send packet
	SendBuffer send = {ipAddr, port, rawTxData, std::move(packet), callback};
	
	//Move it back to sending queue
	sending.enqueue(std::move(send));
	
	//Signal the thread this will cause the poll call to exit
	Signal();
}

std::optional<uint16_t> NetEventLoop::GetPollEventMask(int fd) const
{
	//If we have anything to send set to wait also for write events
	return sending.size_approx() ? (Poll::Event::In | Poll::Event::Out) : Poll::Event::In;
}

void NetEventLoop::OnPollIn(int fd)
{
	struct sockaddr_in froms[MaxMultipleReceivingMessages] = {};
	struct mmsghdr messages[MaxMultipleReceivingMessages] = {};
	struct iovec iovs[MaxMultipleReceivingMessages][1] = {{}};

	TRACE_EVENT("neteventloop", "NetEventLoop::OnPollIn");
	//UltraDebug("-NetEventLoop::Run() | ufds[0].revents & POLLIN\n");

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
		for (size_t i = 0; int(i) < len && i < MaxMultipleReceivingMessages; i++)
			//double check
			if (messages[i].msg_len)
				//Run callback
				listener->OnRead(fd, datas[i], messages[i].msg_len, ntohl(froms[i].sin_addr.s_addr), ntohs(froms[i].sin_port));
}

void NetEventLoop::OnPollOut(int fd)
{
	//Multiple messages struct
	struct mmsghdr messages[MaxMultipleSendingMessages] = {};
	struct sockaddr_in tos[MaxMultipleSendingMessages] = {};
	struct iovec iovs[MaxMultipleSendingMessages][1] = {{}};

	TRACE_EVENT("neteventloop", "NetEventLoop::OnPollOut");
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
		TRACE_EVENT("neteventloop", "sendmmsg", "fd", fd, "vlen", len);
		sendmmsg(sendFd, messages, len, flags);
	}
	
	//Update now
	auto now = Now();

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

void NetEventLoop::OnPollError(int fd, const std::string& errorMsg)
{
	throw std::runtime_error("Error occurred on network fd: " + errorMsg);
}
